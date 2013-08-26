/* pack.c --- FSFS shard packing functionality
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */
#include <assert.h>

#include "svn_pools.h"
#include "svn_dirent_uri.h"
#include "svn_sorts.h"
#include "private/svn_temp_serializer.h"
#include "private/svn_subr_private.h"
#include "private/svn_string_private.h"

#include "fs_fs.h"
#include "pack.h"
#include "util.h"
#include "id.h"
#include "low_level.h"
#include "revprops.h"
#include "transaction.h"

#include "../libsvn_fs/fs-loader.h"

#include "svn_private_config.h"
#include "temp_serializer.h"

/* Directories entries sorted by revision (decreasing - to max cache hits)
 * and offset (increasing - to max benefit from APR file buffering).
 */
static int
compare_dir_entries_format6(const svn_sort__item_t *a,
                            const svn_sort__item_t *b)
{
  const svn_fs_dirent_t *lhs = (const svn_fs_dirent_t *) a->value;
  const svn_fs_dirent_t *rhs = (const svn_fs_dirent_t *) b->value;

  const svn_fs_fs__id_part_t *lhs_rev_item
    = svn_fs_fs__id_rev_offset(lhs->id);
  const svn_fs_fs__id_part_t *rhs_rev_item
    = svn_fs_fs__id_rev_offset(rhs->id);

  /* decreasing ("reverse") order on revs */
  if (lhs_rev_item->revision != rhs_rev_item->revision)
    return lhs_rev_item->revision > rhs_rev_item->revision ? -1 : 1;

  /* increasing order on offsets */
  if (lhs_rev_item->number != rhs_rev_item->number)
    return lhs_rev_item->number > rhs_rev_item->number ? 1 : -1;

  return 0;
}

apr_array_header_t *
svn_fs_fs__order_dir_entries(svn_fs_t *fs,
                             apr_hash_t *directory,
                             apr_pool_t *pool)
{
  apr_array_header_t *ordered
    = svn_sort__hash(directory, compare_dir_entries_format6, pool);

  apr_array_header_t *result
    = apr_array_make(pool, ordered->nelts, sizeof(svn_fs_dirent_t *));

  int i;
  for (i = 0; i < ordered->nelts; ++i)
    APR_ARRAY_PUSH(result, svn_fs_dirent_t *)
      = APR_ARRAY_IDX(ordered, i, svn_sort__item_t).value;

  return result;
}

/* Given REV in FS, set *REV_OFFSET to REV's offset in the packed file.
   Use POOL for temporary allocations. */
svn_error_t *
svn_fs_fs__get_packed_offset(apr_off_t *rev_offset,
                             svn_fs_t *fs,
                             svn_revnum_t rev,
                             apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_stream_t *manifest_stream;
  svn_boolean_t is_cached;
  svn_revnum_t shard;
  apr_int64_t shard_pos;
  apr_array_header_t *manifest;
  apr_pool_t *iterpool;

  shard = rev / ffd->max_files_per_dir;

  /* position of the shard within the manifest */
  shard_pos = rev % ffd->max_files_per_dir;

  /* fetch exactly that element into *rev_offset, if the manifest is found
     in the cache */
  SVN_ERR(svn_cache__get_partial((void **) rev_offset, &is_cached,
                                 ffd->packed_offset_cache, &shard,
                                 svn_fs_fs__get_sharded_offset, &shard_pos,
                                 pool));

  if (is_cached)
      return SVN_NO_ERROR;

  /* Open the manifest file. */
  SVN_ERR(svn_stream_open_readonly(&manifest_stream,
                                   svn_fs_fs__path_rev_packed(fs, rev,
                                                              PATH_MANIFEST,
                                                              pool),
                                   pool, pool));

  /* While we're here, let's just read the entire manifest file into an array,
     so we can cache the entire thing. */
  iterpool = svn_pool_create(pool);
  manifest = apr_array_make(pool, ffd->max_files_per_dir, sizeof(apr_off_t));
  while (1)
    {
      svn_boolean_t eof;
      apr_int64_t val;

      svn_pool_clear(iterpool);
      SVN_ERR(svn_fs_fs__read_number_from_stream(&val, &eof, manifest_stream,
                                                 iterpool));
      if (eof)
        break;

      APR_ARRAY_PUSH(manifest, apr_off_t) = (apr_off_t)val;
    }
  svn_pool_destroy(iterpool);

  *rev_offset = APR_ARRAY_IDX(manifest, rev % ffd->max_files_per_dir,
                              apr_off_t);

  /* Close up shop and cache the array. */
  SVN_ERR(svn_stream_close(manifest_stream));
  return svn_cache__set(ffd->packed_offset_cache, &shard, manifest, pool);
}

/* Packing logic:  Simply concatenate all revision contents.
 * 
 * Pack the revision shard starting at SHARD_REV containing exactly
 * MAX_FILES_PER_DIR revisions from SHARD_PATH into the PACK_FILE_DIR,
 * using POOL for allocations.  CANCEL_FUNC and CANCEL_BATON are what you
 * think they are.
 */
static svn_error_t *
pack_phys_addressed(const char *pack_file_dir,
                    const char *shard_path,
                    svn_revnum_t start_rev,
                    int max_files_per_dir,
                    svn_cancel_func_t cancel_func,
                    void *cancel_baton,
                    apr_pool_t *pool)
{
  const char *pack_file_path, *manifest_file_path;
  svn_stream_t *pack_stream, *manifest_stream;
  svn_revnum_t end_rev, rev;
  apr_off_t next_offset;
  apr_pool_t *iterpool;

  /* Some useful paths. */
  pack_file_path = svn_dirent_join(pack_file_dir, PATH_PACKED, pool);
  manifest_file_path = svn_dirent_join(pack_file_dir, PATH_MANIFEST, pool);

  /* Create the new directory and pack file. */
  SVN_ERR(svn_stream_open_writable(&pack_stream, pack_file_path, pool,
                                    pool));

  /* Create the manifest file. */
  SVN_ERR(svn_stream_open_writable(&manifest_stream, manifest_file_path,
                                   pool, pool));

  end_rev = start_rev + max_files_per_dir - 1;
  next_offset = 0;
  iterpool = svn_pool_create(pool);

  /* Iterate over the revisions in this shard, squashing them together. */
  for (rev = start_rev; rev <= end_rev; rev++)
    {
      svn_stream_t *rev_stream;
      apr_finfo_t finfo;
      const char *path;

      svn_pool_clear(iterpool);

      /* Get the size of the file. */
      path = svn_dirent_join(shard_path, apr_psprintf(iterpool, "%ld", rev),
                             iterpool);
      SVN_ERR(svn_io_stat(&finfo, path, APR_FINFO_SIZE, iterpool));

      /* build manifest */
      SVN_ERR(svn_stream_printf(manifest_stream, iterpool,
                                "%" APR_OFF_T_FMT "\n", next_offset));
      next_offset += finfo.size;

      /* Copy all the bits from the rev file to the end of the pack file. */
      SVN_ERR(svn_stream_open_readonly(&rev_stream, path, iterpool, iterpool));
      SVN_ERR(svn_stream_copy3(rev_stream, svn_stream_disown(pack_stream,
                                                             iterpool),
                               cancel_func, cancel_baton, iterpool));
    }

  /* disallow write access to the manifest file */
  SVN_ERR(svn_stream_close(manifest_stream));
  SVN_ERR(svn_io_set_file_read_only(manifest_file_path, FALSE, iterpool));

  SVN_ERR(svn_stream_close(pack_stream));

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* In filesystem FS, pack the revision SHARD containing exactly
 * MAX_FILES_PER_DIR revisions from SHARD_PATH into the PACK_FILE_DIR,
 * using POOL for allocations.  Try to limit the amount of temporary
 * memory needed to MAX_MEM bytes.  CANCEL_FUNC and CANCEL_BATON are what
 * you think they are.
 *
 * If for some reason we detect a partial packing already performed, we
 * remove the pack file and start again.
 *
 * The actual packing will be done in a format-specific sub-function.
 */
static svn_error_t *
pack_rev_shard(svn_fs_t *fs,
               const char *pack_file_dir,
               const char *shard_path,
               apr_int64_t shard,
               int max_files_per_dir,
               apr_size_t max_mem,
               svn_cancel_func_t cancel_func,
               void *cancel_baton,
               apr_pool_t *pool)
{
  const char *pack_file_path;
  svn_revnum_t shard_rev = (svn_revnum_t) (shard * max_files_per_dir);

  /* Some useful paths. */
  pack_file_path = svn_dirent_join(pack_file_dir, PATH_PACKED, pool);

  /* Remove any existing pack file for this shard, since it is incomplete. */
  SVN_ERR(svn_io_remove_dir2(pack_file_dir, TRUE, cancel_func, cancel_baton,
                             pool));

  /* Create the new directory and pack file. */
  SVN_ERR(svn_io_dir_make(pack_file_dir, APR_OS_DEFAULT, pool));

  /* Index information files */
  SVN_ERR(pack_phys_addressed(pack_file_dir, shard_path, shard_rev,
                              max_files_per_dir, cancel_func,
                              cancel_baton, pool));
  
  SVN_ERR(svn_io_copy_perms(shard_path, pack_file_dir, pool));
  SVN_ERR(svn_io_set_file_read_only(pack_file_path, FALSE, pool));

  return SVN_NO_ERROR;
}

/* In the file system at FS_PATH, pack the SHARD in REVS_DIR and
 * REVPROPS_DIR containing exactly MAX_FILES_PER_DIR revisions, using POOL
 * for allocations.  REVPROPS_DIR will be NULL if revprop packing is not
 * supported.  COMPRESSION_LEVEL and MAX_PACK_SIZE will be ignored in that
 * case.
 *
 * CANCEL_FUNC and CANCEL_BATON are what you think they are; similarly
 * NOTIFY_FUNC and NOTIFY_BATON.
 *
 * If for some reason we detect a partial packing already performed, we
 * remove the pack file and start again.
 */
static svn_error_t *
pack_shard(const char *revs_dir,
           const char *revsprops_dir,
           svn_fs_t *fs,
           apr_int64_t shard,
           int max_files_per_dir,
           apr_off_t max_pack_size,
           int compression_level,
           svn_fs_pack_notify_t notify_func,
           void *notify_baton,
           svn_cancel_func_t cancel_func,
           void *cancel_baton,
           apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  const char *rev_shard_path, *rev_pack_file_dir;
  const char *revprops_shard_path, *revprops_pack_file_dir;

  /* Notify caller we're starting to pack this shard. */
  if (notify_func)
    SVN_ERR(notify_func(notify_baton, shard, svn_fs_pack_notify_start,
                        pool));

  /* Some useful paths. */
  rev_pack_file_dir = svn_dirent_join(revs_dir,
                  apr_psprintf(pool,
                               "%" APR_INT64_T_FMT PATH_EXT_PACKED_SHARD,
                               shard),
                  pool);
  rev_shard_path = svn_dirent_join(revs_dir,
                           apr_psprintf(pool, "%" APR_INT64_T_FMT, shard),
                           pool);

  /* pack the revision content */
  SVN_ERR(pack_rev_shard(fs, rev_pack_file_dir, rev_shard_path,
                         shard, max_files_per_dir, 64 * 1024 * 1024,
                         cancel_func, cancel_baton, pool));

  /* if enabled, pack the revprops in an equivalent way */
  if (revsprops_dir)
    {
      revprops_pack_file_dir = svn_dirent_join(revsprops_dir,
                   apr_psprintf(pool,
                                "%" APR_INT64_T_FMT PATH_EXT_PACKED_SHARD,
                                shard),
                   pool);
      revprops_shard_path = svn_dirent_join(revsprops_dir,
                           apr_psprintf(pool, "%" APR_INT64_T_FMT, shard),
                           pool);

      SVN_ERR(svn_fs_fs__pack_revprops_shard(revprops_pack_file_dir,
                                             revprops_shard_path,
                                             shard, max_files_per_dir,
                                             (int)(0.9 * max_pack_size),
                                             compression_level,
                                             cancel_func, cancel_baton,
                                             pool));
    }

  /* Update the min-unpacked-rev file to reflect our newly packed shard. */
  SVN_ERR(svn_fs_fs__write_min_unpacked_rev(fs,
                          (svn_revnum_t)((shard + 1) * max_files_per_dir),
                          pool));
  ffd->min_unpacked_rev = (svn_revnum_t)((shard + 1) * max_files_per_dir);

  /* Finally, remove the existing shard directories.
   * For revprops, clean up older obsolete shards as well as they might
   * have been left over from an interrupted FS upgrade. */
  SVN_ERR(svn_io_remove_dir2(rev_shard_path, TRUE,
                             cancel_func, cancel_baton, pool));
  if (revsprops_dir)
    {
      svn_node_kind_t kind = svn_node_dir;
      apr_int64_t to_cleanup = shard;
      do
        {
          SVN_ERR(svn_fs_fs__delete_revprops_shard(revprops_shard_path,
                                                   to_cleanup,
                                                   max_files_per_dir,
                                                   cancel_func,
                                                   cancel_baton,
                                                   pool));

          /* If the previous shard exists, clean it up as well.
             Don't try to clean up shard 0 as it we can't tell quickly
             whether it actually needs cleaning up. */
          revprops_shard_path = svn_dirent_join(revsprops_dir,
                      apr_psprintf(pool, "%" APR_INT64_T_FMT, --to_cleanup),
                      pool);
          SVN_ERR(svn_io_check_path(revprops_shard_path, &kind, pool));
        }
      while (kind == svn_node_dir && to_cleanup > 0);
    }

  /* Notify caller we're starting to pack this shard. */
  if (notify_func)
    SVN_ERR(notify_func(notify_baton, shard, svn_fs_pack_notify_end,
                        pool));

  return SVN_NO_ERROR;
}

struct pack_baton
{
  svn_fs_t *fs;
  svn_fs_pack_notify_t notify_func;
  void *notify_baton;
  svn_cancel_func_t cancel_func;
  void *cancel_baton;
};


/* The work-horse for svn_fs_fs__pack, called with the FS write lock.
   This implements the svn_fs_fs__with_write_lock() 'body' callback
   type.  BATON is a 'struct pack_baton *'.

   WARNING: if you add a call to this function, please note:
     The code currently assumes that any piece of code running with
     the write-lock set can rely on the ffd->min_unpacked_rev and
     ffd->min_unpacked_revprop caches to be up-to-date (and, by
     extension, on not having to use a retry when calling
     svn_fs_fs__path_rev_absolute() and friends).  If you add a call
     to this function, consider whether you have to call
     svn_fs_fs__update_min_unpacked_rev().
     See this thread: http://thread.gmane.org/1291206765.3782.3309.camel@edith
 */
static svn_error_t *
pack_body(void *baton,
          apr_pool_t *pool)
{
  struct pack_baton *pb = baton;
  fs_fs_data_t *ffd = pb->fs->fsap_data;
  apr_int64_t completed_shards;
  apr_int64_t i;
  svn_revnum_t youngest;
  apr_pool_t *iterpool;
  const char *rev_data_path;
  const char *revprops_data_path = NULL;

  /* If the repository isn't a new enough format, we don't support packing.
     Return a friendly error to that effect. */
  if (ffd->format < SVN_FS_FS__MIN_PACKED_FORMAT)
    return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
      _("FSFS format (%d) too old to pack; please upgrade the filesystem."),
      ffd->format);

  /* If we aren't using sharding, we can't do any packing, so quit. */
  if (!ffd->max_files_per_dir)
    return SVN_NO_ERROR;

  SVN_ERR(svn_fs_fs__read_min_unpacked_rev(&ffd->min_unpacked_rev, pb->fs,
                                           pool));

  SVN_ERR(svn_fs_fs__youngest_rev(&youngest, pb->fs, pool));
  completed_shards = (youngest + 1) / ffd->max_files_per_dir;

  /* See if we've already completed all possible shards thus far. */
  if (ffd->min_unpacked_rev == (completed_shards * ffd->max_files_per_dir))
    return SVN_NO_ERROR;

  rev_data_path = svn_dirent_join(pb->fs->path, PATH_REVS_DIR, pool);
  if (ffd->format >= SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT)
    revprops_data_path = svn_dirent_join(pb->fs->path, PATH_REVPROPS_DIR,
                                         pool);

  iterpool = svn_pool_create(pool);
  for (i = ffd->min_unpacked_rev / ffd->max_files_per_dir;
       i < completed_shards;
       i++)
    {
      svn_pool_clear(iterpool);

      if (pb->cancel_func)
        SVN_ERR(pb->cancel_func(pb->cancel_baton));

      SVN_ERR(pack_shard(rev_data_path, revprops_data_path,
                         pb->fs, i, ffd->max_files_per_dir,
                         ffd->revprop_pack_size,
                         ffd->compress_packed_revprops
                           ? SVN__COMPRESSION_ZLIB_DEFAULT
                           : SVN__COMPRESSION_NONE,
                         pb->notify_func, pb->notify_baton,
                         pb->cancel_func, pb->cancel_baton, iterpool));
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__pack(svn_fs_t *fs,
                svn_fs_pack_notify_t notify_func,
                void *notify_baton,
                svn_cancel_func_t cancel_func,
                void *cancel_baton,
                apr_pool_t *pool)
{
  struct pack_baton pb = { 0 };
  pb.fs = fs;
  pb.notify_func = notify_func;
  pb.notify_baton = notify_baton;
  pb.cancel_func = cancel_func;
  pb.cancel_baton = cancel_baton;
  return svn_fs_fs__with_write_lock(fs, pack_body, &pb, pool);
}
