/*
 * swigutil_pl.h :  utility functions and stuff for the SWIG Perl bindings
 *
 * ====================================================================
 * Copyright (c) 2000-2004 CollabNet.  All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at http://subversion.tigris.org/license-1.html.
 * If newer versions of this license are posted there, you may use a
 * newer version instead, at your option.
 *
 * This software consists of voluntary contributions made by many
 * individuals.  For exact contribution history, see the revision
 * history and logs, available at http://subversion.tigris.org/.
 * ====================================================================
 */


#ifndef SVN_SWIG_SWIGUTIL_PL_H
#define SVN_SWIG_SWIGUTIL_PL_H

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include <apr.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_hash.h>
#include <apr_tables.h>

#include "svn_types.h"
#include "svn_string.h"
#include "svn_delta.h"
#include "svn_client.h"
#include "svn_repos.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if _MSC_VER >= 1300 && _INTEGRAL_MAX_BITS >= 64
#  define strtoll _strtoi64
#elif defined(_MSC_VER)
#  define strtoll(str, endptr, base) _atoi64(str)
#endif


/* If this file is being included outside of a wrapper file, then need to
   create stubs for some of the SWIG types. */

/* if SWIGEXPORT is defined, then we're in a wrapper. otherwise, we need
   the prototypes and type definitions. */
#ifndef SWIGEXPORT
#define SVN_NEED_SWIG_TYPES
#endif

#ifdef SVN_NEED_SWIG_TYPES

#if SVN_SWIG_VERSION >= 103020
#include "perl5/precommon.swg"
/* The following two aren't handled by the precommon.swg.  */
#ifndef SWIG_MakePtr
#define SWIG_MakePtr SWIG_Perl_MakePtr
#endif
#ifndef SWIG_ConvertPtr
#define SWIG_ConvertPtr SWIG_Perl_ConvertPtr
#endif
#endif

typedef struct _unnamed swig_type_info;

swig_type_info *SWIG_TypeQuery(const char *name);
int SWIG_ConvertPtr(SV *, void **, swig_type_info *, int flags);
void SWIG_MakePtr(SV *, void *, swig_type_info *, int flags);

#endif /* SVN_NEED_SWIG_TYPES */

extern apr_pool_t *current_pool;
apr_pool_t *svn_swig_pl_make_pool (SV *obj);

typedef enum perl_func_invoker {
    CALL_METHOD,
    CALL_SV
} perl_func_invoker_t;

svn_error_t *svn_swig_pl_callback_thunk (perl_func_invoker_t caller_func,
		                         void *func,
					 SV **result,
					 const char *fmt, ...);

SV *svn_swig_pl_prophash_to_hash (apr_hash_t *hash);
SV *svn_swig_pl_convert_hash (apr_hash_t *hash, swig_type_info *tinfo);

const apr_array_header_t *svn_swig_pl_strings_to_array(SV *source,
                                                       apr_pool_t *pool);

apr_hash_t *svn_swig_pl_strings_to_hash(SV *source,
                                        apr_pool_t *pool);
apr_hash_t *svn_swig_pl_objs_to_hash(SV *source, swig_type_info *tinfo,
                                     apr_pool_t *pool);
apr_hash_t *svn_swig_pl_objs_to_hash_by_name(SV *source,
                                             const char *typename,
                                             apr_pool_t *pool);
const apr_array_header_t *svn_swig_pl_objs_to_array(SV *source,
                                                    swig_type_info *tinfo,
                                                    apr_pool_t *pool);

SV *svn_swig_pl_array_to_list(const apr_array_header_t *array);
SV *svn_swig_pl_ints_to_list(const apr_array_header_t *array);
SV *svn_swig_pl_convert_array(const apr_array_header_t *array,
                              swig_type_info *tinfo);

/* thunked log receiver function.  */
svn_error_t * svn_swig_pl_thunk_log_receiver(void *py_receiver,
                                             apr_hash_t *changed_paths,
                                             svn_revnum_t rev,
                                             const char *author,
                                             const char *date,
                                             const char *msg,
                                             apr_pool_t *pool);
/* thunked commit editor callback. */
svn_error_t *svn_swig_pl_thunk_commit_callback(svn_revnum_t new_revision,
					       const char *date,
					       const char *author,
					       void *baton);

/* thunked repos_history callback. */
svn_error_t *svn_swig_pl_thunk_history_func(void *baton,
                                            const char *path,
                                            svn_revnum_t revision,
                                            apr_pool_t *pool);

/* thunked dir_delta authz read function. */
svn_error_t *svn_swig_pl_thunk_authz_func (svn_boolean_t *allowed,
                                           svn_fs_root_t *root,
                                           const char *path,
                                           void *baton,
                                           apr_pool_t *pool);

/* ra callbacks. */
svn_error_t *svn_ra_make_callbacks(svn_ra_callbacks_t **cb,
				   void **c_baton,
				   SV *perl_callbacks,
				   apr_pool_t *pool);

/* thunked simple_prompt callback function */
svn_error_t *svn_swig_pl_thunk_simple_prompt (svn_auth_cred_simple_t **cred,
                                              void *baton,
                                              const char *realm,
                                              const char *username,
                                              svn_boolean_t may_save,
                                              apr_pool_t *pool);

/* thunked username_prompt callback function */
svn_error_t *svn_swig_pl_thunk_username_prompt (svn_auth_cred_username_t **cred,
                                                void *baton,
                                                const char *realm,
                                                svn_boolean_t may_save,
                                                apr_pool_t *pool);

/* thunked ssl_server_trust_prompt callback function */
svn_error_t *svn_swig_pl_thunk_ssl_server_trust_prompt(
                              svn_auth_cred_ssl_server_trust_t **cred,
                              void *baton,
                              const char *realm,
                              apr_uint32_t failures,
                              const svn_auth_ssl_server_cert_info_t *cert_info,
                              svn_boolean_t may_save,
                              apr_pool_t *pool);

/* thunked ssl_client_cert callback function */
svn_error_t *svn_swig_pl_thunk_ssl_client_cert_prompt (
                                        svn_auth_cred_ssl_client_cert_t **cred,
                                        void *baton,
                                        const char *realm,
                                        svn_boolean_t may_save,
                                        apr_pool_t *pool);

/* thunked ssl_client_cert_pw callback function */
svn_error_t *svn_swig_pl_thunk_ssl_client_cert_pw_prompt (
                                     svn_auth_cred_ssl_client_cert_pw_t **cred,
                                     void *baton,
                                     const char *realm,
                                     svn_boolean_t may_save,
                                     apr_pool_t *pool);

/* thunked callback for svn_ra_get_wc_prop_func_t */
svn_error_t *thunk_get_wc_prop (void *baton,
                                const char *relpath,
                                const char *name,
                                const svn_string_t **value,
                                apr_pool_t *pool);

/* Thunked version of svn_wc_notify_func_t callback type */
void svn_swig_pl_notify_func(void * baton,
                             const char *path,
		             svn_wc_notify_action_t action,
			     svn_node_kind_t kind,
			     const char *mime_type,
			     svn_wc_notify_state_t content_state,
			     svn_wc_notify_state_t prop_state,
			     svn_revnum_t revision);


/* Thunked version of svn_client_get_commit_log_t callback type. */
svn_error_t *svn_swig_pl_get_commit_log_func(const char **log_msg,
                                             const char **tmp_file,
                                             apr_array_header_t *commit_items,
                                             void *baton,
                                             apr_pool_t *pool);

/* Thunked version of svn_wc_cancel_func_t callback type. */
svn_error_t *svn_swig_pl_cancel_func(void *cancel_baton);

/* Thunked version of svn_wc_status_func_t callback type. */
void svn_swig_pl_status_func(void *baton,
                             const char *path,
                             svn_wc_status_t *status);
/* Thunked version of svn_client_blame_receiver_t callback type. */
svn_error_t *svn_swig_pl_blame_func (void *baton,
                                     apr_int64_t line_no,
                                     svn_revnum_t revision,
                                     const char *author,
                                     const char *date,
                                     const char *line,
                                     apr_pool_t *pool);


/* helper for making the editor */
void svn_delta_make_editor(svn_delta_editor_t **editor,
                           void **edit_baton,
                           SV *perl_editor,
                           apr_pool_t *pool);

/* svn_stream_t helpers */
svn_error_t *svn_swig_pl_make_stream (svn_stream_t **stream, SV *obj);
SV *svn_swig_pl_from_stream (svn_stream_t *stream);

/* apr_file_t * */
apr_file_t *svn_swig_pl_make_file (SV *file, apr_pool_t *pool);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* SVN_SWIG_SWIGUTIL_PL_H */
