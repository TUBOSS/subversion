/*
 * adm_files.h :  handles locations inside the wc adm area
 *                (This should be the only code that actually knows
 *                *where* things are in .svn/.  If you can't get to
 *                something via these interfaces, something's wrong.)
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


#ifndef SVN_LIBSVN_WC_ADM_FILES_H
#define SVN_LIBSVN_WC_ADM_FILES_H

#include <apr_pools.h>
#include "svn_types.h"

#include "props.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/* Return a path to CHILD in the administrative area of PATH. If CHILD is
   NULL, then the path to the admin area is returned. The result is
   allocated in RESULT_POOL. */
const char *svn_wc__adm_child(const char *path,
                              const char *child,
                              apr_pool_t *result_pool);

/* Return TRUE if the administrative area exists for this directory. */
svn_boolean_t svn_wc__adm_area_exists(const svn_wc_adm_access_t *adm_access,
                                      apr_pool_t *pool);


/* Create a killme file in the administrative area, indicating that the
   directory containing the administrative area should be removed.

   If ADM_ONLY is true then remove only the administrative areas for the
   directory and subdirectories. */
svn_error_t *svn_wc__make_killme(svn_wc_adm_access_t *adm_access,
                                 svn_boolean_t adm_only,
                                 apr_pool_t *pool);

/* Set EXISTS to TRUE if a killme file exists in the administrative area,
   FALSE otherwise.

   If EXISTS is true, set KILL_ADM_ONLY to the value passed to
   svn_wc__make_killme() above. */
svn_error_t *svn_wc__check_killme(svn_wc_adm_access_t *adm_access,
                                  svn_boolean_t *exists,
                                  svn_boolean_t *kill_adm_only,
                                  apr_pool_t *pool);

/* Atomically rename a temporary text-base file to its canonical
   location.  The tmp file should be closed already. */
svn_error_t *
svn_wc__sync_text_base(const char *path, apr_pool_t *pool);


/* Return an absolute path to LOCAL_ABSPATH's text-base file.
   If TMP is set, return a path to the tmp text-base file. */
svn_error_t *
svn_wc__text_base_path(const char **result_path,
                       svn_wc__db_t *db,
                       const char *local_abspath,
                       svn_boolean_t tmp,
                       apr_pool_t *pool);

/* Return a readonly stream on the LOCAL_ABSPATH's base file. */
svn_error_t *
svn_wc__get_pristine_contents(svn_stream_t **contents,
                              svn_wc__db_t *db,
                              const char *local_abspath,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool);



/* Return a readonly stream on the LOCAL_ABSPATH's revert file. */
svn_error_t *
svn_wc__get_revert_contents(svn_stream_t **contents,
                            svn_wc__db_t *db,
                            const char *local_abspath,
                            apr_pool_t *result_pool,
                            apr_pool_t *scratch_pool);


/* Retrieve an absolute path to LOCAL_ABSPATH's revert file.
   If TMP is set, return a path to the tmp revert file. */
svn_error_t *
svn_wc__text_revert_path(const char **result_abspath,
                         svn_wc__db_t *db,
                         const char *local_abspath,
                         apr_pool_t *pool);

/* Set *PROP_PATH to PATH's PROPS_KIND properties file.
   PATH can be a directory or file, and even have changed w.r.t. the
   working copy's adm knowledge. Valid values for NODE_KIND are svn_node_dir
   and svn_node_file. */
svn_error_t *svn_wc__prop_path(const char **prop_path,
                               const char *path,
                               svn_node_kind_t node_kind,
                               svn_wc__props_kind_t props_kind,
                               apr_pool_t *pool);



/*** Opening all kinds of adm files ***/

/* Yo, read this if you open and close files in the adm area:
 *
 * ### obsolete documentation. see implementation for now. this entire
 * ### section is likely to be tossed out "soon".
 *
 * When you open a file for writing with svn_wc__open_foo(), the file
 * is actually opened in the corresponding location in the tmp/
 * directory.  Opening with APR_APPEND is not supported.  You are
 * guaranteed to be the owner of the new file.
 *
 * Somehow, this tmp file must eventually get renamed to its real
 * destination in the adm area.  You can do it either by passing the
 * SYNC flag to svn_wc__close_foo(), or by calling
 * svn_wc__sync_foo() (though of course you should still have
 * called svn_wc__close_foo() first, just without the SYNC flag).
 *
 * In other words, the adm area is only capable of modifying files
 * atomically, but you get some control over when the rename happens.
 */

/* Open `PATH/<adminstrative_subdir>/FNAME'. Note: STREAM and TEMP_FILE_PATH
   should be passed to svn_wc__close_adm_stream when you're done writing. */
svn_error_t *svn_wc__open_adm_writable(svn_stream_t **stream,
                                       const char **temp_file_path,
                                       const char *path,
                                       const char *fname,
                                       apr_pool_t *result_pool,
                                       apr_pool_t *scratch_pool);

/* Close `PATH/<adminstrative_subdir>/FNAME'. */
svn_error_t *svn_wc__close_adm_stream(svn_stream_t *stream,
                                      const char *temp_file_path,
                                      const char *path,
                                      const char *fname,
                                      apr_pool_t *scratch_pool);

/* Open `PATH/<adminstrative_subdir>/FNAME'. */
svn_error_t *svn_wc__open_adm_stream(svn_stream_t **stream,
                                     const char *path,
                                     const char *fname,
                                     apr_pool_t *result_pool,
                                     apr_pool_t *scratch_pool);


/* Remove `DIR_PATH/<adminstrative_subdir>/FILENAME'. */
svn_error_t *svn_wc__remove_adm_file(const char *dir_path,
                                     const char *filename,
                                     apr_pool_t *scratch_pool);


/* Open the normal or revert text base, associated with PATH, for writing.
   The selection is based on NEED_REVERT_BASE. The opened stream will be
   returned in STREAM and the selected path will be returned in,
   TEMP_BASE_PATH, and both will be allocated in RESULT_POOL. Any temporary
   allocations will be performed in SCRATCH_POOL. */
svn_error_t *
svn_wc__open_writable_base(svn_stream_t **stream,
                           const char **temp_base_path,
                           const char *path,
                           svn_boolean_t need_revert_base,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool);


/* Blow away the admistrative directory associated with the access baton
   ADM_ACCESS. This closes ADM_ACCESS, but it is safe to close ADM_ACCESS
   again, after calling this function. */
svn_error_t *svn_wc__adm_destroy(svn_wc_adm_access_t *adm_access,
                                 apr_pool_t *scratch_pool);


/* Cleanup the temporary storage area of the administrative
   directory (assuming temp and admin areas exist). */
svn_error_t *
svn_wc__adm_cleanup_tmp_area(const svn_wc_adm_access_t *adm_access,
                             apr_pool_t *scratch_pool);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_WC_ADM_FILES_H */
