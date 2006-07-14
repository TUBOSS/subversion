/*
 * ====================================================================
 * Copyright (c) 2000-2006 CollabNet.  All rights reserved.
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
 *
 * svn_repos.i: SWIG interface file for svn_repos.h
 */

#if defined(SWIGPYTHON)
%module(package="libsvn") repos
#elif defined(SWIGPERL)
%module "SVN::_Repos"
#elif defined(SWIGRUBY)
%module "svn::ext::repos"
#endif

%include svn_global.swg
%import core.i
%import svn_delta.i
%import svn_fs.i

/* -----------------------------------------------------------------------
   %apply-ing of typemaps defined elsewhere
*/
%apply const char *MAY_BE_NULL {
    const char *src_entry,
    const char *unused_1,
    const char *unused_2,
    const char *token,
    const char *user,
    const char *log_msg,
    const char *lock_token,
    const char *tgt_path
};

#ifdef SWIGPYTHON
%apply svn_stream_t *WRAPPED_STREAM { svn_stream_t * };
#endif

%callback_typemap(svn_repos_history_func_t history_func, void *history_baton,
                  svn_swig_py_repos_history_func,
                  svn_swig_pl_thunk_history_func,
                  svn_swig_rb_repos_history_func)

%callback_typemap_maybenull(svn_repos_authz_func_t authz_read_func,
                            void *authz_read_baton,
                            svn_swig_py_repos_authz_func,
                            svn_swig_pl_thunk_authz_func,
                            svn_swig_rb_repos_authz_func)

/* cause SWIG syntax error.
#ifdef SWIGRUBY
%typemap(in) (svn_error_t *(*)(void *baton) start_callback, void *start_callback_baton)
{
  $1 = svn_swig_rb_just_call;
  $2 = (void *)svn_swig_rb_make_baton($input, _global_svn_swig_rb_pool);
}
#endif
*/

#ifdef SWIGRUBY
%callback_typemap(svn_repos_file_rev_handler_t handler, void *handler_baton,
                  ,
                  ,
                  svn_swig_rb_repos_file_rev_handler)

%callback_typemap(svn_repos_authz_func_t authz_read_func,
                  void *authz_read_baton,
                  ,
                  ,
                  svn_swig_rb_repos_authz_func)

%callback_typemap(svn_repos_authz_callback_t authz_callback, void *authz_baton,
                  ,
                  ,
                  svn_swig_rb_repos_authz_callback)
#endif

/* Ruby fixups for functions not following the pool convention. */
#ifdef SWIGRUBY
%ignore svn_repos_fs;
%inline %{
static svn_fs_t *
svn_repos_fs_wrapper(svn_repos_t *fs, apr_pool_t *pool)
{
  return svn_repos_fs(fs);
}
%}
#endif

/* ----------------------------------------------------------------------- */

%include svn_repos_h.swg
