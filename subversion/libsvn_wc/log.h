/*
 * log.h :  interfaces for running .svn/log files.
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


#ifndef SVN_LIBSVN_WC_LOG_H
#define SVN_LIBSVN_WC_LOG_H

#include <apr_pools.h>

#include "svn_types.h"
#include "svn_error.h"
#include "svn_wc.h"

#include "wc_db.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/* OVERVIEW OF THE LOGGY API
 *
 * NOTES
 *
 *  * When a doc string says "Extend **LOG_ACCUM", it means: "if *LOG_ACCUM is
 *    NULL then set *LOG_ACCUM to a new stringbuf allocated in RESULT_POOL,
 *    else append to the existing stringbuf there."
 */

/* Each path argument to the svn_wc__loggy_* functions in this section can
   be either absolute or relative to the adm_abspath argument.
*/


/* A macro to flush LOG_ACCUM using DB and ADM_ABSPATH.  This writes all
   current items in LOG_ACCUM to the work queue, and then reinitializes
   LOG_ACCUM to an empty buffer. */
#define SVN_WC__FLUSH_LOG_ACCUM(db, adm_abspath, log_accum, scratch_pool)   \
  if (!svn_stringbuf_isempty(log_accum))                                    \
    {                                                                       \
      SVN_ERR(svn_wc__wq_add_loggy(db, adm_abspath, log_accum, scratch_pool));\
      svn_stringbuf_setempty(log_accum);                                    \
    }                                                                       \
  else {}


/* Extend **LOG_ACCUM with log instructions to append the contents
   of SRC to DST.
   SRC and DST are relative to ADM_ABSPATH.

   This command fails to be idempotent or atomic: there's no way to
   tell if you should re-run this!  This function is deprecated; new
   uses should not be added, and the single current use (constructing
   human-readable non-parsed property conflict files) should be
   rewritten.  See Issue #3015.
*/
SVN_DEPRECATED
svn_error_t *
svn_wc__loggy_append(svn_stringbuf_t **log_accum,
                     const char *adm_abspath,
                     const char *src, const char *dst,
                     apr_pool_t *pool);


/* Extend **LOG_ACCUM with log instructions to copy (and translate!) the
   file SRC_PATH to DST_PATH, if it exists. If it doesn't and
   REMOVE_DST_IF_NO_SRC is TRUE the file at DST_PATH will be deleted if any.

   The test for existence is made during this call, not at log running time.

   ADM_ABSPATH is the absolute path for the admin directory for PATH.
   SRC_PATH and DST_PATH are relative to ADM_ABSPATH.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_copy(svn_stringbuf_t **log_accum,
                   const char *adm_abspath,
                   const char *src_path, const char *dst_path,
                   apr_pool_t *result_pool,
                   apr_pool_t *scratch_pool);


/* Extend **LOG_ACCUM with log instructions to generate a translated
   file from SRC to DST with translation settings from VERSIONED.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.
   DST and SRC and VERSIONED are relative to ADM_ABSPATH.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_translated_file(svn_stringbuf_t **log_accum,
                              const char *adm_abspath,
                              const char *dst,
                              const char *src,
                              const char *versioned,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool);

/* Insert into DB a work queue instruction to delete the entry
   associated with PATH from the entries file.
   ADM_ABSPATH is the absolute path for the access baton for PATH.

   Use SCRATCH_POOL for temporary allocations.
*/
svn_error_t *
svn_wc__loggy_delete_entry(svn_wc__db_t *db,
                           const char *adm_abspath,
                           const char *path,
                           apr_pool_t *scratch_pool);


/* Insert into DB a work queue instruction to delete lock related
   fields from the entry belonging to PATH.
   ADM_ABSPATH is the absolute path for the access baton for PATH.

   Use SCRATCH_POOL for temporary allocations.
*/
svn_error_t *
svn_wc__loggy_delete_lock(svn_wc__db_t *db,
                          const char *adm_abspath,
                          const char *path,
                          apr_pool_t *scratch_pool);


/* Extend **LOG_ACCUM with commands to modify the entry associated with PATH
   in ADM_ABSPATH according to the flags specified in MODIFY_FLAGS, based on
   the values supplied in *ENTRY.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   The flags in MODIFY_FLAGS are to be taken from the svn_wc__entry_modify()
   parameter by the same name.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_entry_modify(svn_stringbuf_t **log_accum,
                           const char *adm_abspath,
                           const char *path,
                           const svn_wc_entry_t *entry,
                           apr_uint64_t modify_flags,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool);


/* Extend **LOG_ACCUM with log instructions to move the file SRC_PATH to
   DST_PATH, if it exists. If it doesn't and REMOVE_DST_IF_NO_SRC is TRUE
   the file at DST_PATH will be deleted if any.
   TODO ### There is no 'REMOVE_DST_IF_NO_SRC' arg; what will happen?

   The test for existence is made now, not at log run time.

   ADM_ABSPATH is the absolute path for the admin directory for PATH.
   SRC_PATH and DST_PATH are relative to ADM_ABSPATH.

   Set *DST_MODIFIED (if DST_MODIFIED isn't NULL) to indicate whether the
   destination path will have been modified after running the log: if either
   the move or the remove will have been carried out.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_move(svn_stringbuf_t **log_accum,
                   const char *adm_abspath,
                   const char *src_path, const char *dst_path,
                   apr_pool_t *result_pool,
                   apr_pool_t *scratch_pool);



/* Extend **LOG_ACCUM with log instructions to set permissions of PATH
   to 'executable' if it has the 'executable' property set.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   The property is tested at log run time, within this log instruction.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_maybe_set_executable(svn_stringbuf_t **log_accum,
                                   const char *adm_abspath,
                                   const char *path,
                                   apr_pool_t *result_pool,
                                   apr_pool_t *scratch_pool);

/* Extend **LOG_ACCUM with log instructions to set permissions of PATH
   to 'readonly' if it has the 'needs-lock' property set and there is
   no lock for the file in the working copy.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   The tests are made at log run time, within this log instruction.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_maybe_set_readonly(svn_stringbuf_t **log_accum,
                                 const char *adm_abspath,
                                 const char *path,
                                 apr_pool_t *result_pool,
                                 apr_pool_t *scratch_pool);


/* Extend **LOG_ACCUM with log instructions to set the timestamp of PATH
   in the entry field with name TIME_PROP.
   TODO ### Huh? There is no 'TIME_PROP' argument.

   Use one of the SVN_WC__ENTRY_ATTR_* values for TIME_PROP.

   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_set_entry_timestamp_from_wc(svn_stringbuf_t **log_accum,
                                          const char *adm_abspath,
                                          const char *path,
                                          apr_pool_t *result_pool,
                                          apr_pool_t *scratch_pool);


/* Extend **LOG_ACCUM with log instructions to set the file size of PATH
   in the entries' WORKING_SIZE field.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_set_entry_working_size_from_wc(svn_stringbuf_t **log_accum,
                                             const char *adm_abspath,
                                             const char *path,
                                             apr_pool_t *result_pool,
                                             apr_pool_t *scratch_pool);


/* Extend **LOG_ACCUM with log instructions to set permissions of PATH
   to 'readonly'.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_set_readonly(svn_stringbuf_t **log_accum,
                           const char *adm_abspath,
                           const char *path,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool);

/* Extend **LOG_ACCUM with log instructions to set the timestamp of PATH to
   the time TIMESTR.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_set_timestamp(svn_stringbuf_t **log_accum,
                            const char *adm_abspath,
                            const char *path,
                            const char *timestr,
                            apr_pool_t *result_pool,
                            apr_pool_t *scratch_pool);

/* Extend **LOG_ACCUM with log instructions to remove the file
   PATH, if it exists.
   ADM_ABSPATH is the absolute path for the admin directory for PATH.

   Allocate *LOG_ACCUM in RESULT_POOL if it is NULL. Use SCRATCH_POOL for
   temporary allocations.
*/
svn_error_t *
svn_wc__loggy_remove(svn_stringbuf_t **log_accum,
                     const char *adm_abspath,
                     const char *path,
                     apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool);


/* Process the instructions in the log file for ADM_ABSPATH,
   using its access baton that MUST BE cached in DB.

   Use SCRATCH_POOL for temporary allocations.
 */
svn_error_t *
svn_wc__run_log2(svn_wc__db_t *db,
                 const char *adm_abspath,
                 apr_pool_t *scratch_pool);


/* TODO ###

   Use SCRATCH_POOL for temporary allocations.
 */
svn_error_t *
svn_wc__run_xml_log(svn_wc__db_t *db,
                    const char *adm_abspath,
                    const char *log_contents,
                    apr_size_t log_len,
                    apr_pool_t *scratch_pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_WC_LOG_H */
