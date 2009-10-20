/*
 * entries.h :  manipulating entries
 *
 * ====================================================================
 *    Licensed to the Subversion Corporation (SVN Corp.) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The SVN Corp. licenses this file
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


#ifndef SVN_LIBSVN_WC_ENTRIES_H
#define SVN_LIBSVN_WC_ENTRIES_H

#include <apr_pools.h>

#include "svn_types.h"

#include "wc_db.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* String representations for svn_node_kind.  This maybe should be
   abstracted farther out? */
#define SVN_WC__ENTRIES_ATTR_FILE_STR   "file"
#define SVN_WC__ENTRIES_ATTR_DIR_STR    "dir"


/* The names of the fields used for storing entries' information.
   Used for the names of the XML attributes in XML entries files
   (format 6 and below), for the names of attributes in wc logs,
   and for error reporting when reading a non-XML entries file.
   ### If you add or remove items here, you probably want to make sure
   to do the same for the SVN_WC__ENTRY_MODIFY_* #defines as well. */
#define SVN_WC__ENTRY_ATTR_NAME               "name"
#define SVN_WC__ENTRY_ATTR_REVISION           "revision"
#define SVN_WC__ENTRY_ATTR_URL                "url"
#define SVN_WC__ENTRY_ATTR_KIND               "kind"
#define SVN_WC__ENTRY_ATTR_TEXT_TIME          "text-time"
#define SVN_WC__ENTRY_ATTR_CHECKSUM           "checksum"
#define SVN_WC__ENTRY_ATTR_SCHEDULE           "schedule"
#define SVN_WC__ENTRY_ATTR_COPIED             "copied"
#define SVN_WC__ENTRY_ATTR_DELETED            "deleted"
#define SVN_WC__ENTRY_ATTR_ABSENT             "absent"
#define SVN_WC__ENTRY_ATTR_COPYFROM_URL       "copyfrom-url"
#define SVN_WC__ENTRY_ATTR_COPYFROM_REV       "copyfrom-rev"
#define SVN_WC__ENTRY_ATTR_CONFLICT_OLD       "conflict-old" /* saved old file */
#define SVN_WC__ENTRY_ATTR_CONFLICT_NEW       "conflict-new" /* saved new file */
#define SVN_WC__ENTRY_ATTR_CONFLICT_WRK       "conflict-wrk" /* saved wrk file */
#define SVN_WC__ENTRY_ATTR_PREJFILE           "prop-reject-file"
#define SVN_WC__ENTRY_ATTR_CMT_REV            "committed-rev"
#define SVN_WC__ENTRY_ATTR_CMT_DATE           "committed-date"
#define SVN_WC__ENTRY_ATTR_CMT_AUTHOR         "last-author"
#define SVN_WC__ENTRY_ATTR_WORKING_SIZE       "working-size"

/* Attribute values for 'schedule' */
#define SVN_WC__ENTRY_VALUE_ADD        "add"
#define SVN_WC__ENTRY_VALUE_DELETE     "delete"
#define SVN_WC__ENTRY_VALUE_REPLACE    "replace"



/* Initialize an entries file based on URL at INITIAL_REV, in the adm
   area for PATH.  The adm area must not already have an entries
   file.  UUID is the repository UUID, and may be NULL.  REPOS is the
   repository root URL and, if not NULL, must be a prefix of URL.
   DEPTH is the initial depth of the working copy, it must be a
   definite depth, not svn_depth_unknown.

   If initial rev is valid and non-zero, then mark the 'this_dir'
   entry as being incomplete.
*/
svn_error_t *svn_wc__entries_init(const char *path,
                                  const char *uuid,
                                  const char *url,
                                  const char *repos,
                                  svn_revnum_t initial_rev,
                                  svn_depth_t depth,
                                  apr_pool_t *pool);


/* Set *NEW_ENTRY to a new entry, taking attributes from ATTS, whose
   keys and values are both char *.  Allocate the entry and copy
   attributes into POOL as needed.

   Set MODIFY_FLAGS to reflect the fields that were present in ATTS. */
svn_error_t *svn_wc__atts_to_entry(svn_wc_entry_t **new_entry,
                                   apr_uint64_t *modify_flags,
                                   apr_hash_t *atts,
                                   apr_pool_t *pool);


/* The MODIFY_FLAGS that tell svn_wc__entry_modify which parameters to
   pay attention to.  ### These should track the changes made to the
   SVN_WC__ENTRY_ATTR_* #defines! */
/* Note: we use APR_INT64_C because APR 0.9 lacks APR_UINT64_C */
#define SVN_WC__ENTRY_MODIFY_REVISION           APR_INT64_C(0x0000000000000001)
#define SVN_WC__ENTRY_MODIFY_URL                APR_INT64_C(0x0000000000000002)
/* OPEN */
#define SVN_WC__ENTRY_MODIFY_KIND               APR_INT64_C(0x0000000000000008)
#define SVN_WC__ENTRY_MODIFY_TEXT_TIME          APR_INT64_C(0x0000000000000010)
/* OPEN */
#define SVN_WC__ENTRY_MODIFY_CHECKSUM           APR_INT64_C(0x0000000000000040)
#define SVN_WC__ENTRY_MODIFY_SCHEDULE           APR_INT64_C(0x0000000000000080)
#define SVN_WC__ENTRY_MODIFY_COPIED             APR_INT64_C(0x0000000000000100)
#define SVN_WC__ENTRY_MODIFY_DELETED            APR_INT64_C(0x0000000000000200)
#define SVN_WC__ENTRY_MODIFY_COPYFROM_URL       APR_INT64_C(0x0000000000000400)
#define SVN_WC__ENTRY_MODIFY_COPYFROM_REV       APR_INT64_C(0x0000000000000800)
#define SVN_WC__ENTRY_MODIFY_CONFLICT_OLD       APR_INT64_C(0x0000000000001000)
#define SVN_WC__ENTRY_MODIFY_CONFLICT_NEW       APR_INT64_C(0x0000000000002000)
#define SVN_WC__ENTRY_MODIFY_CONFLICT_WRK       APR_INT64_C(0x0000000000004000)
#define SVN_WC__ENTRY_MODIFY_PREJFILE           APR_INT64_C(0x0000000000008000)
#define SVN_WC__ENTRY_MODIFY_CMT_REV            APR_INT64_C(0x0000000000010000)
#define SVN_WC__ENTRY_MODIFY_CMT_DATE           APR_INT64_C(0x0000000000020000)
#define SVN_WC__ENTRY_MODIFY_CMT_AUTHOR         APR_INT64_C(0x0000000000040000)
/* OPEN */
#define SVN_WC__ENTRY_MODIFY_INCOMPLETE         APR_INT64_C(0x0000000000100000)
#define SVN_WC__ENTRY_MODIFY_ABSENT             APR_INT64_C(0x0000000000200000)
/* OPEN */
#define SVN_WC__ENTRY_MODIFY_KEEP_LOCAL         APR_INT64_C(0x0000000080000000)
#define SVN_WC__ENTRY_MODIFY_WORKING_SIZE       APR_INT64_C(0x0000000100000000)
/* OPEN */
#define SVN_WC__ENTRY_MODIFY_FILE_EXTERNAL      APR_INT64_C(0x0000000400000000)
/* No #define for DEPTH, because it's only meaningful on this-dir anyway. */

/* ...ORed together with this to mean: just set the schedule to the new
   value, instead of treating the new value as a change of state to be
   merged with the current schedule. */
#define SVN_WC__ENTRY_MODIFY_FORCE              APR_INT64_C(0x4000000000000000)


/* TODO ### Rewrite doc string to mention DB, LOCAL_ABSPATH; not ADM_ACCESS, NAME.

   Modify an entry for NAME in access baton ADM_ACCESS by folding in
   ("merging") changes, and sync those changes to disk.  New values
   for the entry are pulled from their respective fields in ENTRY, and
   MODIFY_FLAGS is a bitmask to specify which of those fields to pay
   attention to, formed from the values SVN_WC__ENTRY_MODIFY_....
   ADM_ACCESS must hold a write lock.

   NAME can be NULL to specify that the caller wishes to modify the
   "this dir" entry in ADM_ACCESS.

   "Folding in" a change means, in most cases, simply replacing the field
   with the new value. However, for the "schedule" field, unless
   MODIFY_FLAGS includes SVN_WC__ENTRY_MODIFY_FORCE (in which case just take
   the new schedule from ENTRY), it means to determine the schedule that the
   entry should end up with if the "schedule" value from ENTRY represents a
   change/add/delete/replace being made to the
     ### base / working / base and working version(s) ?
   of the node.

   Perform all allocations in SCRATCH_POOL.

   -----

   A cross between svn_wc__get_entry() and svn_wc__entry_modify().

   If PARENT_STUB is TRUE, then this function will modify a directory's
   stub entry in the parent. If PARENT_STUB is FALSE, then it will operate
   on a directory's real entry.

   PARENT_STUB must be FALSE if KIND==FILE.

   If KIND is svn_kind_unknown, then PARENT_STUB is interpreted based on
   what is found on disk.  */
svn_error_t *
svn_wc__entry_modify2(svn_wc__db_t *db,
                      const char *local_abspath,
                      svn_node_kind_t kind,
                      svn_boolean_t parent_stub,
                      svn_wc_entry_t *entry,
                      apr_uint64_t modify_flags,
                      apr_pool_t *scratch_pool);


/* Remove LOCAL_ABSPATH from DB, unconditionally.

   All temporary allocations will be performed in SCRATCH_POOL.  */
svn_error_t *
svn_wc__entry_remove(svn_wc__db_t *db,
                     const char *local_abspath,
                     apr_pool_t *scratch_pool);


/* Tweak the information for LOCAL_ABSPATH in DB.  If NEW_URL is non-null,
 * make this the entry's new url.  If NEW_REV is valid, make this the
 * entry's working revision.
 *
 * If ALLOW_REMOVAL is TRUE the tweaks might cause the entry NAME to
 * be removed from the hash, if ALLOW_REMOVAL is FALSE this will not
 * happen.
 *
 * THIS_DIR should be true if the LOCAL_ABSPATH refers to a directory, and
 * the information to be edited is not in the stub entry.
 *
 * (Intended as a helper to svn_wc__do_update_cleanup, which see.)
 */
svn_error_t *
svn_wc__tweak_entry(svn_wc__db_t *db,
                    const char *local_abspath,
                    svn_node_kind_t kind,
                    svn_boolean_t parent_stub,
                    const char *new_url,
                    svn_revnum_t new_rev,
                    svn_boolean_t allow_removal,
                    apr_pool_t *scratch_pool);


/** Get an ENTRY for the given LOCAL_ABSPATH.
 *
 * This API does not require an access baton, just a wc_db handle (DB).
 * The requested entry MUST be present and version-controlled when
 * ALLOW_UNVERSIONED is FALSE; otherwise, SVN_ERR_WC_PATH_NOT_FOUND is
 * returned. When ALLOW_UNVERSIONED is TRUE, and the node is not under
 * version control, *ENTRY will be set to NULL (this is easier for callers
 * to handle, than detecting the error and clearing it).
 *
 * If you know the entry is a FILE or DIR, then specify that in KIND. If you
 * are unsure, then specific 'svn_node_unknown' for KIND. This value will be
 * used to optimize the access to the entry, so it is best to know the kind.
 * If you specify FILE/DIR, and the entry is *something else*, then
 * SVN_ERR_NODE_UNEXPECTED_KIND will be returned.
 *
 * For directory nodes, sometimes the caller may want the "stub" from the
 * parent directory. This is usually to examine the DELETED flag. When
 * this is desired, pass TRUE for NEED_PARENT_STUB. It is illegal to pass
 * TRUE if KIND == FILE.
 *
 * If KIND == UNKNOWN, and you request the parent stub, and the node turns
 * out to NOT be a directory, then SVN_ERR_NODE_UNEXPECTED_KIND is returned.
 *
 * If KIND == UNKNOWN, and you request the actual file/dir data (by setting
 * NEED_PARENT_STUB to FALSE), and the node turns out to be a DIR (as
 * specified by the parent), but the subdirectory is NOT present (obstructed
 * or missing), then SVN_ERR_NODE_UNEXPECTED_KIND is returned.
 *
 * NOTE: if SVN_ERR_NODE_UNEXPECTED_KIND is returned, then the ENTRY *IS*
 * valid and may be examined. For any other error, ENTRY *IS NOT* valid.
 *
 * NOTE: if an access baton is available, then it will be examined for
 * cached entries (and this routine may even cache them for you). It is
 * not required, however, to do any access baton management for this API.
 *
 * ENTRY will be allocated in RESULT_POOL, and all temporary allocations
 * will be performed in SCRATCH_POOL.
 */
svn_error_t *
svn_wc__get_entry(const svn_wc_entry_t **entry,
                  svn_wc__db_t *db,
                  const char *local_abspath,
                  svn_boolean_t allow_unversioned,
                  svn_node_kind_t kind,
                  svn_boolean_t need_parent_stub,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool);


/* Is ENTRY in a 'hidden' state in the sense of the 'show_hidden'
 * switches on svn_wc_entries_read(), svn_wc_walk_entries*(), etc.? */
svn_error_t *
svn_wc__entry_is_hidden(svn_boolean_t *hidden, const svn_wc_entry_t *entry);


/* Set the depth of a directory.  */
svn_error_t *
svn_wc__set_depth(svn_wc__db_t *db,
                  const char *local_dir_abspath,
                  svn_depth_t depth,
                  apr_pool_t *scratch_pool);


/* For internal use by entries.c to read/write old-format working copies. */
svn_error_t *
svn_wc__read_entries_old(apr_hash_t **entries,
                         const char *path,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool);

/* For internal use by upgrade.c to write entries in the wc-ng format.  */
svn_error_t *
svn_wc__entries_write_new(svn_wc__db_t *db,
                          const char *dir_abspath,
                          apr_hash_t *entries,
                          apr_pool_t *scratch_pool);


/* ### return a flag corresponding to the classic "DELETED" concept.  */
svn_error_t *
svn_wc__node_is_deleted(svn_boolean_t *deleted,
                        svn_wc__db_t *db,
                        const char *local_abspath,
                        apr_pool_t *scratch_pool);


/* Parse a file external specification in the NULL terminated STR and
   place the path in PATH_RESULT, the peg revision in PEG_REV_RESULT
   and revision number in REV_RESULT.  STR may be NULL, in which case
   PATH_RESULT will be set to NULL and both PEG_REV_RESULT and
   REV_RESULT set to svn_opt_revision_unspecified.

   The format that is read is the same as a working-copy path with a
   peg revision; see svn_opt_parse_path(). */
svn_error_t *
svn_wc__unserialize_file_external(const char **path_result,
                                  svn_opt_revision_t *peg_rev_result,
                                  svn_opt_revision_t *rev_result,
                                  const char *str,
                                  apr_pool_t *pool);

/* Serialize into STR the file external path, peg revision number and
   the operative revision number into a format that
   unserialize_file_external() can parse.  The format is
     %{peg_rev}:%{rev}:%{path}
   where a rev will either be HEAD or the string revision number.  If
   PATH is NULL then STR will be set to NULL.  This method writes to a
   string instead of a svn_stringbuf_t so that the string can be
   protected by write_str(). */
svn_error_t *
svn_wc__serialize_file_external(const char **str,
                                const char *path,
                                const svn_opt_revision_t *peg_rev,
                                const svn_opt_revision_t *rev,
                                apr_pool_t *pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_WC_ENTRIES_H */
