/*
 * props.h :  properties
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


#ifndef SVN_LIBSVN_WC_PROPS_H
#define SVN_LIBSVN_WC_PROPS_H

#include <apr_pools.h>

#include "svn_types.h"
#include "svn_string.h"
#include "svn_props.h"

#include "wc_db.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* BASE_MERGE is a pre-1.7 concept on property merging. It allowed callers
   to alter the pristine properties *outside* of an editor drive. That is
   very dangerous: the pristines should always correspond to something from
   the repository, and that should only arrive through the update editor.

   For 1.7, we're removing this support. Some old code is being left around
   in case we decide to change this.

   For more information, see ^/notes/api-errata/1.7/wc006.txt
*/
#undef SVN__SUPPORT_BASE_MERGE

/* Internal function for diffing props. See svn_wc_get_prop_diffs2(). */
svn_error_t *
svn_wc__internal_propdiff(apr_array_header_t **propchanges,
                          apr_hash_t **original_props,
                          svn_wc__db_t *db,
                          const char *local_abspath,
                          apr_pool_t *result_pool,
                          apr_pool_t *scratch_pool);


/* Internal function for fetching a property. See svn_wc_prop_get2(). */
svn_error_t *
svn_wc__internal_propget(const svn_string_t **value,
                         svn_wc__db_t *db,
                         const char *local_abspath,
                         const char *name,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool);


/* Given LOCAL_ABSPATH/DB and an array of PROPCHANGES based on
   SERVER_BASEPROPS, calculate what changes should be applied to the working
   copy.

   We return the new property collections to the caller, so the caller
   can combine the property update with other operations.

   If SERVER_BASEPROPS is NULL then use the pristine props as PROPCHANGES
   base.

   Return the new set of actual properties in *NEW_ACTUAL_PROPS.  If
   NEW_PRISTINE_PROPS is non-null, then also apply PROPCHANGES to
   PRISTINE_PROPS and return the new set of pristine properties in
   *NEW_PRISTINE_PROPS.

   Append any conflicts of the actual props to *CONFLICT_SKEL.  (First
   allocate *CONFLICT_SKEL from RESULT_POOL if it is initially NULL.
   CONFLICT_SKEL itself must not be NULL.)  (Changes made to the pristine
   properties, if BASE_MERGE is TRUE, do not generate conficts.)

   If STATE is non-null, set *STATE to the state of the local properties
   after the merge.  */
svn_error_t *
svn_wc__merge_props(svn_skel_t **conflict_skel,
                    svn_wc_notify_state_t *state,
                    apr_hash_t **new_pristine_props,
                    apr_hash_t **new_actual_props,
                    svn_wc__db_t *db,
                    const char *local_abspath,
                    /*const*/ apr_hash_t *server_baseprops,
                    /*const*/ apr_hash_t *pristine_props,
                    /*const*/ apr_hash_t *actual_props,
                    const apr_array_header_t *propchanges,
                    apr_pool_t *result_pool,
                    apr_pool_t *scratch_pool);


/* Given PROPERTIES is array of @c svn_prop_t structures. Returns TRUE if any
   of the PROPERTIES are the known "magic" ones that might require
   changing the working file. */
svn_boolean_t svn_wc__has_magic_property(const apr_array_header_t *properties);

/* Set *MODIFIED_P TRUE if the props for LOCAL_ABSPATH have been modified. */
svn_error_t *
svn_wc__props_modified(svn_boolean_t *modified_p,
                       svn_wc__db_t *db,
                       const char *local_abspath,
                       apr_pool_t *scratch_pool);

/* Internal version of svn_wc_prop_list2().  */
svn_error_t *
svn_wc__get_actual_props(apr_hash_t **props,
                         svn_wc__db_t *db,
                         const char *local_abspath,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool);

svn_error_t *
svn_wc__create_prejfile(const char **tmp_prejfile_abspath,
                        svn_wc__db_t *db,
                        const char *local_abspath,
                        const svn_skel_t *conflict_skel,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool);


/* Just like svn_wc_merge_props3(), but WITH a BASE_MERGE parameter.

   If SVN__SUPPORT_BASE_MERGE is defined and BASE_MERGE is true, then
   also use PROPCHANGES to modify the node's pristine properties.  (That
   cannot generate conficts.)  If SVN__SUPPORT_BASE_MERGE is not defined
   and BASE_MERGE is true, throw an error.
 */
svn_error_t *
svn_wc__perform_props_merge(svn_wc_notify_state_t *state,
                            svn_wc__db_t *db,
                            const char *local_abspath,
                            const svn_wc_conflict_version_t *left_version,
                            const svn_wc_conflict_version_t *right_version,
                            apr_hash_t *baseprops,
                            const apr_array_header_t *propchanges,
                            svn_boolean_t base_merge,
                            svn_boolean_t dry_run,
                            svn_wc_conflict_resolver_func2_t conflict_func,
                            void *conflict_baton,
                            svn_cancel_func_t cancel_func,
                            void *cancel_baton,
                            apr_pool_t *scratch_pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_WC_PROPS_H */
