/*
 * adm_files.c: helper routines for handling files & dirs in the
 *              working copy administrative area (creating,
 *              deleting, opening, and closing).  This is the only
 *              code that actually knows where administrative
 *              information is kept.
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



#include <stdarg.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_strings.h>

#include "svn_types.h"
#include "svn_error.h"
#include "svn_io.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_hash.h"

#include "wc.h"
#include "adm_files.h"
#include "entries.h"
#include "lock.h"

#include "svn_private_config.h"
#include "private/svn_wc_private.h"


/*** File names in the adm area. ***/

/* The default name of the WC admin directory. This name is always
   checked by svn_wc_is_adm_dir. */
static const char default_adm_dir_name[] = ".svn";

/* The name that is actually used for the WC admin directory.  The
   commonest case where this won't be the default is in Windows
   ASP.NET development environments, which choke on ".svn". */
static const char *adm_dir_name = default_adm_dir_name;


svn_boolean_t
svn_wc_is_adm_dir(const char *name, apr_pool_t *pool)
{
  return (0 == strcmp(name, adm_dir_name)
          || 0 == strcmp(name, default_adm_dir_name));
}


const char *
svn_wc_get_adm_dir(apr_pool_t *pool)
{
  return adm_dir_name;
}


svn_error_t *
svn_wc_set_adm_dir(const char *name, apr_pool_t *pool)
{
  /* This is the canonical list of administrative directory names.

     FIXME:
     An identical list is used in
       libsvn_subr/opt.c:svn_opt__args_to_target_array(),
     but that function can't use this list, because that use would
     create a circular dependency between libsvn_wc and libsvn_subr.
     Make sure changes to the lists are always synchronized! */
  static const char *valid_dir_names[] = {
    default_adm_dir_name,
    "_svn",
    NULL
  };

  const char **dir_name;
  for (dir_name = valid_dir_names; *dir_name; ++dir_name)
    if (0 == strcmp(name, *dir_name))
      {
        /* Use the pointer to the statically allocated string
           constant, to avoid potential pool lifetime issues. */
        adm_dir_name = *dir_name;
        return SVN_NO_ERROR;
      }
  return svn_error_createf(SVN_ERR_BAD_FILENAME, NULL,
                           _("'%s' is not a valid administrative "
                             "directory name"),
                           svn_dirent_local_style(name, pool));
}


static const char *
simple_extend(const char *adm_path,  /* ### adm_abspath?  */
              svn_boolean_t use_tmp,
              const char *subdir,
              const char *child,
              const char *extension,
              apr_pool_t *result_pool)
{
  if (subdir)
    child = svn_dirent_join(subdir, child, result_pool);
  if (extension)
    child = apr_pstrcat(result_pool, child, extension, NULL);

  if (use_tmp)
    return svn_dirent_join_many(result_pool,
                                adm_path,
                                adm_dir_name,
                                SVN_WC__ADM_TMP,
                                child,
                                NULL);

  return svn_dirent_join_many(result_pool,
                              adm_path,
                              adm_dir_name,
                              child,
                              NULL);
}


const char *svn_wc__adm_child(const char *path,
                              const char *child,
                              apr_pool_t *result_pool)
{
  return simple_extend(path, FALSE, NULL, child, NULL, result_pool);
}


svn_boolean_t
svn_wc__adm_area_exists(const char *adm_abspath,
                        apr_pool_t *pool)
{
  const char *path = svn_wc__adm_child(adm_abspath, NULL, pool);
  svn_node_kind_t kind;
  svn_error_t *err;

  err = svn_io_check_path(path, &kind, pool);
  if (err)
    {
      svn_error_clear(err);
      /* Return early, since kind is undefined in this case. */
      return FALSE;
    }

  return kind != svn_node_none;
}



/*** Making and using files in the adm area. ***/


/* */
static svn_error_t *
make_adm_subdir(const char *path,
                const char *subdir,
                svn_boolean_t tmp,
                apr_pool_t *pool)
{
  const char *fullpath;

  fullpath = simple_extend(path, tmp, NULL, subdir, NULL, pool);

  return svn_io_dir_make(fullpath, APR_OS_DEFAULT, pool);
}



/*** Syncing files in the adm area. ***/


svn_error_t *
svn_wc__sync_text_base(svn_wc__db_t *db,
                       const char *local_abspath,
                       const char *tmp_text_base_abspath,
                       apr_pool_t *scratch_pool)
{
  const char *base_path;

  SVN_ERR(svn_wc__text_base_path(&base_path, db, local_abspath, scratch_pool));

  SVN_ERR(svn_io_file_rename(tmp_text_base_abspath, base_path, scratch_pool));
  SVN_ERR(svn_io_set_file_read_only(base_path, FALSE, scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__text_base_path(const char **result_abspath,
                       svn_wc__db_t *db,
                       const char *local_abspath,
                       apr_pool_t *pool)
{
  const char *newpath, *base_name;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  svn_dirent_split(local_abspath, &newpath, &base_name, pool);
  *result_abspath = simple_extend(newpath,
                                  FALSE,
                                  SVN_WC__ADM_TEXT_BASE,
                                  base_name,
                                  SVN_WC__BASE_EXT,
                                  pool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__text_base_deterministic_tmp_path(const char **result_abspath,
                                         svn_wc__db_t *db,
                                         const char *local_abspath,
                                         apr_pool_t *pool)
{
  const char *newpath, *base_name;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  svn_dirent_split(local_abspath, &newpath, &base_name, pool);
  *result_abspath = simple_extend(newpath, TRUE, SVN_WC__ADM_TEXT_BASE,
                                  base_name, SVN_WC__BASE_EXT, pool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__text_revert_path(const char **result_abspath,
                         svn_wc__db_t *db,
                         const char *local_abspath,
                         apr_pool_t *pool)
{
  const char *newpath, *base_name;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  svn_dirent_split(local_abspath, &newpath, &base_name, pool);
  *result_abspath = simple_extend(newpath,
                                  FALSE,
                                  SVN_WC__ADM_TEXT_BASE,
                                  base_name,
                                  SVN_WC__REVERT_EXT,
                                  pool);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__text_base_path_to_read(const char **result_abspath,
                               svn_wc__db_t *db,
                               const char *local_abspath,
                               apr_pool_t *result_pool)
{
  SVN_ERR(svn_wc__text_base_path(result_abspath, db, local_abspath,
                                 result_pool));
  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__text_revert_path_to_read(const char **result_abspath,
                                 svn_wc__db_t *db,
                                 const char *local_abspath,
                                 apr_pool_t *result_pool)
{
  SVN_ERR(svn_wc__text_revert_path(result_abspath, db, local_abspath,
                                   result_pool));
  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__ultimate_base_text_path(const char **result_abspath,
                                svn_wc__db_t *db,
                                const char *local_abspath,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  const svn_wc_entry_t *entry;
  svn_boolean_t replaced;

  SVN_ERR(svn_wc__get_entry(&entry, db, local_abspath, TRUE, svn_node_file,
                            FALSE, scratch_pool, scratch_pool));
  replaced = entry && entry->schedule == svn_wc_schedule_replace;

  if (replaced)
    SVN_ERR(svn_wc__text_revert_path(result_abspath, db, local_abspath,
                                     result_pool));
  else
    SVN_ERR(svn_wc__text_base_path(result_abspath, db, local_abspath,
                                   result_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__ultimate_base_text_path_to_read(const char **result_abspath,
                                        svn_wc__db_t *db,
                                        const char *local_abspath,
                                        apr_pool_t *result_pool,
                                        apr_pool_t *scratch_pool)
{
  return svn_wc__ultimate_base_text_path(result_abspath, db, local_abspath,
                                         result_pool, scratch_pool);
}


svn_error_t *
svn_wc__get_ultimate_base_contents(svn_stream_t **contents,
                                   svn_wc__db_t *db,
                                   const char *local_abspath,
                                   apr_pool_t *result_pool,
                                   apr_pool_t *scratch_pool)
{
#ifdef SVN_EXPERIMENTAL_PRISTINE
  svn_wc__db_kind_t kind;
  svn_wc__db_status_t status;
  const svn_checksum_t *checksum;

  SVN_ERR(svn_wc__db_base_get_info(&status, &kind, NULL, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, &checksum,
                                   NULL, NULL, NULL,
                                   db, local_abspath,
                                   scratch_pool, scratch_pool));
  if (checksum && checksum->kind != svn_checksum_sha1)
    SVN_ERR(svn_wc__db_pristine_get_sha1(&checksum, db, local_abspath,
                                         checksum,
                                         scratch_pool, scratch_pool));
  if (kind != svn_wc__db_kind_file)
    return svn_error_createf(SVN_ERR_WC_NOT_FILE, NULL,
                             _("base node of '%s' is not a file"),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));
  if (status != svn_wc__db_status_normal)
    {
      SVN_ERR_ASSERT(checksum == NULL);
      *contents = NULL;
      return SVN_NO_ERROR;
    }
  SVN_ERR_ASSERT(checksum != NULL);
  SVN_ERR(svn_wc__db_pristine_read(contents, db, local_abspath,
                                   checksum, result_pool, scratch_pool));
  return SVN_NO_ERROR;
#else
  const char *revert_base;
  svn_error_t *err;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  /* If there's a WC-1 "revert base", open that. */
  SVN_ERR(svn_wc__text_revert_path_to_read(&revert_base, db, local_abspath,
                                           scratch_pool));
  err = svn_stream_open_readonly(contents, revert_base,
                                 result_pool, scratch_pool);
  if (err)
    {
      svn_error_clear(err);

      /* There's no "revert base", so open the "normal base". */
      SVN_ERR(svn_wc__text_base_path_to_read(&revert_base, db, local_abspath,
                                             scratch_pool));
      err = svn_stream_open_readonly(contents, revert_base,
                                     result_pool, scratch_pool);
    }
  return err;
#endif
}


svn_error_t *
svn_wc__get_pristine_contents(svn_stream_t **contents,
                              svn_wc__db_t *db,
                              const char *local_abspath,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool)
{
  svn_wc__db_status_t status;
  svn_wc__db_kind_t kind;

  SVN_ERR(svn_wc__db_read_info(&status, &kind,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL,
                               db, local_abspath, scratch_pool, scratch_pool));

  /* Sanity */
  if (kind != svn_wc__db_kind_file)
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("Can only get the pristine contents of files; "
                               "'%s' is not a file"),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));

  if (status == svn_wc__db_status_added)
    {
      /* For an added node, we return "no stream". Make sure this is not
         copied-here or moved-here, in which case we return the copy/move
         source's contents.  */
      SVN_ERR(svn_wc__db_scan_addition(&status,
                                       NULL, NULL, NULL, NULL, NULL, NULL,
                                       NULL, NULL,
                                       db, local_abspath,
                                       scratch_pool, scratch_pool));
      if (status == svn_wc__db_status_added)
        {
          /* Simply added. The pristine base does not exist. */
          *contents = NULL;
          return SVN_NO_ERROR;
        }
    }
  else if (status == svn_wc__db_status_not_present)
    /* We know that the delete of this node has been committed.
       This should be the same as if called on an unknown path. */
    return svn_error_createf(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
                             _("Cannot get the pristine contents of '%s' "
                               "because its delete is already committed"),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));
  else if (status == svn_wc__db_status_absent
      || status == svn_wc__db_status_excluded
      || status == svn_wc__db_status_incomplete)
    return svn_error_createf(SVN_ERR_WC_PATH_UNEXPECTED_STATUS, NULL,
                             _("Cannot get the pristine contents of '%s' "
                               "because it has an unexpected status"),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));
  else
    /* We know that it is a file, so we can't hit the _obstructed stati.
       Also, we should never see _base_deleted here. */
    SVN_ERR_ASSERT(status != svn_wc__db_status_obstructed
                   && status != svn_wc__db_status_obstructed_add
                   && status != svn_wc__db_status_obstructed_delete
                   && status != svn_wc__db_status_base_deleted);

  /* ### TODO 1.7: use pristine store instead of this block: */
  {
    const char *text_base;
    svn_error_t *err;

    SVN_ERR(svn_wc__text_base_path_to_read(&text_base, db, local_abspath,
                                           scratch_pool));
    SVN_ERR_ASSERT(text_base != NULL);

    /* ### now for some ugly hackiness. right now, file externals will
       ### sometimes put their pristine contents into the revert base,
       ### because they think they're *replaced* nodes, rather than
       ### simple BASE nodes. watch out for this scenario, and
       ### compensate appropriately.  */
    err = svn_stream_open_readonly(contents, text_base,
                                   result_pool, scratch_pool);
    if (err)
      {
        svn_boolean_t file_external;

        if (!APR_STATUS_IS_ENOENT(err->apr_err))
          return svn_error_return(err);

        SVN_ERR(svn_wc__internal_is_file_external(&file_external,
                                                  db, local_abspath,
                                                  scratch_pool));
        if (!file_external)
          return svn_error_return(err);

        svn_error_clear(err);

        SVN_ERR(svn_wc__text_revert_path_to_read(&text_base, db, local_abspath,
                                                 scratch_pool));
        return svn_stream_open_readonly(contents, text_base,
                                   result_pool, scratch_pool);
      }
    return SVN_NO_ERROR;
  }
}


svn_error_t *
svn_wc__get_ultimate_base_md5_checksum(const svn_checksum_t **md5_checksum,
                                       svn_wc__db_t *db,
                                       const char *local_abspath,
                                       apr_pool_t *result_pool,
                                       apr_pool_t *scratch_pool)
{
  svn_error_t *err;

  err = svn_wc__db_base_get_info(NULL, NULL, NULL, NULL, NULL, NULL,
                                 NULL, NULL, NULL, NULL, NULL, md5_checksum,
                                 NULL, NULL, NULL,
                                 db, local_abspath,
                                 scratch_pool, scratch_pool);
  if (err && err->apr_err == SVN_ERR_WC_PATH_NOT_FOUND)
    {
      svn_error_clear(err);
      *md5_checksum = NULL;
      return SVN_NO_ERROR;
    }
  if (*md5_checksum && (*md5_checksum)->kind != svn_checksum_md5)
    SVN_ERR(svn_wc__db_pristine_get_md5(md5_checksum, db, local_abspath,
                                        *md5_checksum,
                                        scratch_pool, scratch_pool));
  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__prop_path(const char **prop_path,
                  const char *path,
                  svn_wc__db_kind_t node_kind,
                  svn_wc__props_kind_t props_kind,
                  apr_pool_t *pool)
{
  if (node_kind == svn_wc__db_kind_dir)
    {
      static const char * names[] = {
        SVN_WC__ADM_DIR_PROP_BASE,    /* svn_wc__props_base */
        SVN_WC__ADM_DIR_PROP_REVERT,  /* svn_wc__props_revert */
        SVN_WC__ADM_DIR_PROPS         /* svn_wc__props_working */
      };

      *prop_path = simple_extend(path, FALSE, NULL, names[props_kind], NULL,
                                 pool);
    }
  else  /* It's a file */
    {
      static const char * extensions[] = {
        SVN_WC__BASE_EXT,     /* svn_wc__props_base */
        SVN_WC__REVERT_EXT,   /* svn_wc__props_revert */
        SVN_WC__WORK_EXT      /* svn_wc__props_working */
      };

      static const char * dirs[] = {
        SVN_WC__ADM_PROP_BASE,  /* svn_wc__props_base */
        SVN_WC__ADM_PROP_BASE,  /* svn_wc__props_revert */
        SVN_WC__ADM_PROPS       /* svn_wc__props_working */
      };

      const char *base_name;

      svn_dirent_split(path, prop_path, &base_name, pool);
      *prop_path = simple_extend(*prop_path, FALSE, dirs[props_kind],
                                 base_name, extensions[props_kind], pool);
    }

  return SVN_NO_ERROR;
}


/*** Opening and closing files in the adm area. ***/

svn_error_t *
svn_wc__open_adm_stream(svn_stream_t **stream,
                        const char *dir_abspath,
                        const char *fname,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool)
{
  const char *local_abspath;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(dir_abspath));

  local_abspath = svn_wc__adm_child(dir_abspath, fname, scratch_pool);
  return svn_error_return(svn_stream_open_readonly(stream, local_abspath,
                                                   result_pool, scratch_pool));
}


svn_error_t *
svn_wc__open_writable_base(svn_stream_t **stream,
                           const char **temp_base_abspath,
                           svn_checksum_t **md5_checksum,
                           svn_checksum_t **sha1_checksum,
                           svn_wc__db_t *db,
                           const char *local_abspath,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool)
{
  const char *temp_dir_abspath;
  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  SVN_ERR(svn_wc__db_pristine_get_tempdir(&temp_dir_abspath, db, local_abspath,
                                          scratch_pool, scratch_pool));
  SVN_ERR(svn_stream_open_unique(stream,
                                 temp_base_abspath,
                                 temp_dir_abspath,
                                 svn_io_file_del_none,
                                 result_pool, scratch_pool));
  if (md5_checksum)
    *stream = svn_stream_checksummed2(*stream, NULL, md5_checksum,
                                      svn_checksum_md5, FALSE, result_pool);
  if (sha1_checksum)
    *stream = svn_stream_checksummed2(*stream, NULL, sha1_checksum,
                                      svn_checksum_sha1, FALSE, result_pool);

  return SVN_NO_ERROR;
}



/*** Checking for and creating administrative subdirs. ***/


/* */
static svn_error_t *
init_adm_tmp_area(const char *path, apr_pool_t *pool)
{
  /* SVN_WC__ADM_TMP */
  SVN_ERR(make_adm_subdir(path, SVN_WC__ADM_TMP, FALSE, pool));

  /* SVN_WC__ADM_TMP/SVN_WC__ADM_TEXT_BASE */
  SVN_ERR(make_adm_subdir(path, SVN_WC__ADM_TEXT_BASE, TRUE, pool));

  /* SVN_WC__ADM_TMP/SVN_WC__ADM_PROP_BASE */
  SVN_ERR(make_adm_subdir(path, SVN_WC__ADM_PROP_BASE, TRUE, pool));

  /* SVN_WC__ADM_TMP/SVN_WC__ADM_PROPS */
  return svn_error_return(make_adm_subdir(path, SVN_WC__ADM_PROPS, TRUE,
                                          pool));
}


/* Set up a new adm area for PATH, with REPOS_* as the repos info, and
   INITIAL_REV as the starting revision.  The entries file starts out
   marked as 'incomplete.  The adm area starts out locked; remember to
   unlock it when done. */
static svn_error_t *
init_adm(svn_wc__db_t *db,
         const char *local_abspath,
         const char *repos_relpath,
         const char *repos_root_url,
         const char *repos_uuid,
         svn_revnum_t initial_rev,
         svn_depth_t depth,
         apr_pool_t *pool)
{
  /* First, make an empty administrative area. */
  SVN_ERR(svn_io_dir_make_hidden(svn_wc__adm_child(local_abspath, NULL, pool),
                                 APR_OS_DEFAULT, pool));

  /** Make subdirectories. ***/

  /* SVN_WC__ADM_TEXT_BASE */
  SVN_ERR(make_adm_subdir(local_abspath, SVN_WC__ADM_TEXT_BASE, FALSE, pool));

  /* SVN_WC__ADM_PROP_BASE */
  SVN_ERR(make_adm_subdir(local_abspath, SVN_WC__ADM_PROP_BASE, FALSE, pool));

  /* SVN_WC__ADM_PROPS */
  SVN_ERR(make_adm_subdir(local_abspath, SVN_WC__ADM_PROPS, FALSE, pool));

  /* SVN_WC__ADM_PRISTINE */
  SVN_ERR(make_adm_subdir(local_abspath, SVN_WC__ADM_PRISTINE, FALSE, pool));

  /* ### want to add another directory? do a format bump to ensure that
     ### all existing working copies get the new directories. or maybe
     ### create-on-demand (more expensive)  */

  /** Init the tmp area. ***/
  SVN_ERR(init_adm_tmp_area(local_abspath, pool));

  /* Lastly, create the SDB.  */
  SVN_ERR(svn_wc__db_init(db, local_abspath,
                          repos_relpath, repos_root_url, repos_uuid,
                          initial_rev, depth,
                          pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__internal_ensure_adm(svn_wc__db_t *db,
                            const char *local_abspath,
                            const char *url,
                            const char *repos_root_url,
                            const char *repos_uuid,
                            svn_revnum_t revision,
                            svn_depth_t depth,
                            apr_pool_t *scratch_pool)
{
  const svn_wc_entry_t *entry;
  int format;
  const char *repos_relpath;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));
  SVN_ERR_ASSERT(url != NULL);
  SVN_ERR_ASSERT(repos_root_url != NULL);
  SVN_ERR_ASSERT(repos_uuid != NULL);
  SVN_ERR_ASSERT(svn_uri_is_ancestor(repos_root_url, url));

  SVN_ERR(svn_wc__internal_check_wc(&format, db, local_abspath, scratch_pool));

  repos_relpath = svn_uri_is_child(repos_root_url, url, scratch_pool);
  if (repos_relpath == NULL)
    repos_relpath = "";
  else
    repos_relpath = svn_path_uri_decode(repos_relpath, scratch_pool);

  /* Early out: we know we're not dealing with an existing wc, so
     just create one. */
  if (format == 0)
    return svn_error_return(init_adm(db, local_abspath,
                                     repos_relpath, repos_root_url, repos_uuid,
                                     revision, depth, scratch_pool));

  /* Now, get the existing url and repos for PATH. */
  SVN_ERR(svn_wc__get_entry(&entry, db, local_abspath, FALSE, svn_node_unknown,
                            FALSE, scratch_pool, scratch_pool));

  /* When the directory exists and is scheduled for deletion do not
   * check the revision or the URL.  The revision can be any
   * arbitrary revision and the URL may differ if the add is
   * being driven from a merge which will have a different URL. */
  if (entry->schedule != svn_wc_schedule_delete)
    {
      if (entry->revision != revision)
        return
          svn_error_createf(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
                            _("Revision %ld doesn't match existing "
                              "revision %ld in '%s'"),
                            revision, entry->revision, local_abspath);

      /* The caller gives us a URL which should match the entry. However,
         some callers compensate for an old problem in entry->url and pass
         the copyfrom_url instead. See ^/notes/api-errata/wc002.txt. As
         a result, we allow the passed URL to match copyfrom_url if it
         does match the entry's primary URL.  */
      /** ### comparing URLs, should they be canonicalized first? */
      if (strcmp(entry->url, url) != 0
          && (entry->copyfrom_url == NULL
              || strcmp(entry->copyfrom_url, url) != 0)
          && (!svn_uri_is_ancestor(repos_root_url, entry->url)
              || strcmp(entry->uuid, repos_uuid) != 0))
        {
          return
            svn_error_createf(SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
                              _("URL '%s' doesn't match existing "
                                "URL '%s' in '%s'"),
                              url, entry->url, local_abspath);
        }
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc_ensure_adm4(svn_wc_context_t *wc_ctx,
                   const char *local_abspath,
                   const char *url,
                   const char *repos_root_url,
                   const char *repos_uuid,
                   svn_revnum_t revision,
                   svn_depth_t depth,
                   apr_pool_t *scratch_pool)
{
  return svn_error_return(
    svn_wc__internal_ensure_adm(wc_ctx->db, local_abspath, url, repos_root_url,
                                repos_uuid, revision, depth, scratch_pool));
}

svn_error_t *
svn_wc__adm_destroy(svn_wc__db_t *db,
                    const char *dir_abspath,
                    apr_pool_t *scratch_pool)
{
  const char *adm_abspath;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(dir_abspath));

  SVN_ERR(svn_wc__write_check(db, dir_abspath, scratch_pool));

  /* Well, the coast is clear for blowing away the administrative
     directory, which also removes the lock */
  SVN_ERR(svn_wc__db_temp_forget_directory(db, dir_abspath, scratch_pool));

  adm_abspath = svn_wc__adm_child(dir_abspath, NULL, scratch_pool);
  SVN_ERR(svn_io_remove_dir2(adm_abspath, FALSE, NULL, NULL, scratch_pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__adm_cleanup_tmp_area(svn_wc__db_t *db,
                             const char *adm_abspath,
                             apr_pool_t *scratch_pool)
{
  const char *tmp_path;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(adm_abspath));

  SVN_ERR(svn_wc__write_check(db, adm_abspath, scratch_pool));

  /* Get the path to the tmp area, and blow it away. */
  tmp_path = svn_wc__adm_child(adm_abspath, SVN_WC__ADM_TMP, scratch_pool);

  SVN_ERR(svn_io_remove_dir2(tmp_path, TRUE, NULL, NULL, scratch_pool));

  /* Now, rebuild the tmp area. */
  return svn_error_return(init_adm_tmp_area(adm_abspath, scratch_pool));
}



svn_error_t *
svn_wc_create_tmp_file2(apr_file_t **fp,
                        const char **new_name,
                        const char *path,
                        svn_io_file_del_t delete_when,
                        apr_pool_t *pool)
{
  svn_wc__db_t *db;
  const char *local_abspath;
  const char *temp_dir;
  apr_file_t *file;
  svn_error_t *err;

  SVN_ERR_ASSERT(fp || new_name);

  SVN_ERR(svn_wc__db_open(&db, svn_wc__db_openmode_readonly,
                          NULL /* config */,
                          TRUE /* auto_upgrade */,
                          TRUE /* enforce_empty_wq */,
                          pool, pool));

  SVN_ERR(svn_dirent_get_absolute(&local_abspath, path, pool));
  err = svn_wc__db_temp_wcroot_tempdir(&temp_dir, db, local_abspath,
                                       pool, pool);
  err = svn_error_compose_create(err, svn_wc__db_close(db));
  if (err)
    return svn_error_return(err);

  SVN_ERR(svn_io_open_unique_file3(&file, new_name, temp_dir,
                                   delete_when, pool, pool));

  if (fp)
    *fp = file;
  else
    SVN_ERR(svn_io_file_close(file, pool));

  return SVN_NO_ERROR;
}
