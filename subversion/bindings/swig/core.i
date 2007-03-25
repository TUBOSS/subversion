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
 * core.i: SWIG module interface file for libsvn_subr, a few pieces of
 *   APR functionality, and anything else that does not fit into any
 *   of the more specific module files.
 */

#if defined(SWIGPYTHON)
%module(package="libsvn") core
#elif defined(SWIGPERL)
%module "SVN::_Core"
#elif defined(SWIGRUBY)
%module "svn::ext::core"
#endif

%include svn_global.swg

%{
#include <apr.h>
#include <apr_general.h>

#include "svn_md5.h"
#include "svn_diff.h"
%}

#ifdef SWIGRUBY
%{
#include <apu.h>
#include <apr_xlate.h>
%}
#endif

/* ### for now, let's ignore this thing. */
#ifndef SWIGRUBY
%ignore svn_prop_t;
#endif
#ifdef SWIGRUBY
%immutable svn_prop_t::name;
%immutable svn_prop_t::value;
#endif

/* -----------------------------------------------------------------------
   The following struct members have to be read-only because otherwise
   strings assigned to then would never be freed, resulting in memory
   leaks. This prevents the swig warning "Warning(451): Setting const
   char * member may leak memory."
*/
%immutable svn_log_changed_path_t::copyfrom_path;
%immutable svn_dirent_t::last_author;
%immutable svn_error_t::message;
%immutable svn_error_t::file;

/* -----------------------------------------------------------------------
   completely ignore a number of functions. the presumption is that the
   scripting language already has facilities for these things (or they
   are relatively trivial).
*/

/* svn_io.h: We cherry-pick certain functions from this file. To aid in this,
 * EVERY function in the file is listed in the order it appears, and is either
 * %ignore-d, or present as a comment, explicitly documenting that we wrap it.
 */

%ignore svn_io_check_path;
%ignore svn_io_check_special_path;
%ignore svn_io_check_resolved_path;
/* This is useful for implementing svn_ra_callbacks_t->open_tmp_file */
// svn_io_open_unique_file2
// svn_io_open_unique_file
%ignore svn_io_create_unique_link;
%ignore svn_io_read_link;
%ignore svn_io_temp_dir;
%ignore svn_io_copy_file;
%ignore svn_io_copy_link;
%ignore svn_io_copy_dir_recursively;
%ignore svn_io_make_dir_recursively;
%ignore svn_io_dir_empty;
%ignore svn_io_append_file;
%ignore svn_io_set_file_read_only;
%ignore svn_io_set_file_read_write;
%ignore svn_io_set_file_read_write_carefully;
%ignore svn_io_set_file_executable;
%ignore svn_io_is_file_executable;
%ignore svn_io_read_length_line;
%ignore svn_io_file_affected_time;
%ignore svn_io_set_file_affected_time;
%ignore svn_io_filesizes_different_p;
// svn_io_file_checksum
// svn_io_files_contents_same_p
%ignore svn_io_file_create;
%ignore svn_io_file_lock;
%ignore svn_io_file_lock2;
%ignore svn_io_file_flush_to_disk;
%ignore svn_io_dir_file_copy;

/* Not useful from scripting languages. Custom streams should be achieved
 * by passing a scripting language native stream into a svn_stream_t *
 * parameter, and letting a typemap using svn_swig_xx_make_stream() take
 * care of the details. */
%ignore svn_stream_create;
%ignore svn_stream_set_baton;
%ignore svn_stream_set_read;
%ignore svn_stream_set_write;
%ignore svn_stream_set_close;

/* The permitted svn_stream and svn_stringbuf functions could possibly
 * be used by a script, in conjunction with other APIs which return or
 * accept streams. This requires that the relevant language's custom
 * svn_stream_t wrapping code does not obstruct this usage. */
// svn_stream_empty
// svn_stream_disown
// svn_stream_from_aprfile2
// svn_stream_from_aprfile
// svn_stream_for_stdout
// svn_stream_from_stringbuf
// svn_stream_compressed
/* svn_stream_checksummed would require special attention to wrap, because
 * of the read_digest and write_digest parameters. */
%ignore svn_stream_checksummed;
// svn_stream_read
// svn_stream_write
// svn_stream_close

/* Scripts can do the printf, then write to a stream.
 * We can't really handle the variadic, so ignore it. */
%ignore svn_stream_printf;
%ignore svn_stream_printf_from_utf8;

// svn_stream_readline
// svn_stream_copy
// svn_stream_contents_same
// svn_stringbuf_from_file
// svn_stringbuf_from_aprfile

#ifndef SWIGPYTHON
/* These functions are useful in Python, because they allow you to
 * easily delete files which are marked as read-only on Windows. */
%ignore svn_io_remove_file;
%ignore svn_io_remove_dir;
#endif
%ignore svn_io_get_dir_filenames;
%ignore svn_io_get_dirents2;
%ignore svn_io_get_dirents;
%ignore svn_io_dir_walk;
%ignore svn_io_start_cmd;
%ignore svn_io_wait_for_cmd;
%ignore svn_io_run_cmd;
%ignore svn_io_run_diff;
%ignore svn_io_run_diff3_2;
%ignore svn_io_run_diff3;
// svn_io_detect_mimetype
%ignore svn_io_file_open;
%ignore svn_io_file_close;
%ignore svn_io_file_getc;
%ignore svn_io_file_info_get;
%ignore svn_io_file_read;
%ignore svn_io_file_read_full;
%ignore svn_io_file_seek;
%ignore svn_io_file_write;
%ignore svn_io_file_write_full;
%ignore svn_io_stat;
%ignore svn_io_file_rename;
%ignore svn_io_file_move;
%ignore svn_io_dir_make;
%ignore svn_io_dir_make_hidden;
%ignore svn_io_dir_make_sgid;
%ignore svn_io_dir_open;
%ignore svn_io_dir_remove_nonrecursive;
%ignore svn_io_dir_read;
%ignore svn_io_read_version_file;
%ignore svn_io_write_version_file;

/* svn_path.h: We cherry-pick certain functions from this file. To aid in this,
 * EVERY function in the file is listed in the order it appears, and is either
 * %ignore-d, or present as a comment, explicitly documenting that we wrap it.
 */
%ignore svn_path_internal_style;
%ignore svn_path_local_style;
%ignore svn_path_join;
%ignore svn_path_join_many;
%ignore svn_path_basename;
%ignore svn_path_dirname;
%ignore svn_path_component_count;
%ignore svn_path_add_component;
%ignore svn_path_remove_component;
%ignore svn_path_remove_components;
%ignore svn_path_split;
// svn_path_is_empty;
// svn_path_canonicalize;
// svn_path_compare_paths;
// svn_path_get_longest_ancestor;
%ignore svn_path_get_absolute;
%ignore svn_path_split_if_file;
%ignore svn_path_condense_targets;
%ignore svn_path_remove_redundancies;
%ignore svn_path_decompose;
%ignore svn_path_is_single_path_component;
%ignore svn_path_is_backpath_present;
%ignore svn_path_is_child;
%ignore svn_path_is_ancestor;
%ignore svn_path_check_valid;
%ignore svn_path_is_url;
// svn_path_is_uri_safe;
%ignore svn_path_uri_encode;
%ignore svn_path_uri_decode;
%ignore svn_path_url_add_component;
%ignore svn_path_uri_from_iri;
%ignore svn_path_uri_autoescape;
%ignore svn_path_cstring_from_utf8;
%ignore svn_path_cstring_to_utf8;

/* Other files */
/* bad pool convention */
%ignore svn_opt_print_generic_help;

/* Ugliness because the constants are typedefed and SWIG ignores them
   as a result. */
%constant svn_revnum_t SWIG_SVN_INVALID_REVNUM = -1;
%constant svn_revnum_t SWIG_SVN_IGNORED_REVNUM = -1;

/* -----------------------------------------------------------------------
   input rangelist to svn_rangelist_to_stringbuf
*/
%apply apr_array_header_t *RANGELIST {
   apr_array_header_t *rangeinput
}

/* -----------------------------------------------------------------------
   input mergeinfo hash to svn_mergeinfo_to_stringbuf
*/
%apply apr_hash_t *MERGEINFO {
   apr_hash_t *mergeinput,
   apr_hash_t *mergefrom,
   apr_hash_t *mergeto,
   apr_hash_t *mergein1,
   apr_hash_t *mergein2,
   apr_hash_t *eraser,
   apr_hash_t *whiteboard
}

/* -----------------------------------------------------------------------
   handle the default value of svn_config_get().and the
   config directory of svn_config_read_auth_data() and
   svn_config_write_auth_data().
*/
%apply const char *MAY_BE_NULL {
    const char *default_value,
    const char *config_dir,
    const char *conflict_original,
    const char *conflict_modified,
    const char *conflict_latest,
    const char *conflict_separator
};

/* -----------------------------------------------------------------------
   fix up the svn_stream_read() ptr/len arguments
*/
#ifdef SWIGPYTHON
%typemap(in) (char *buffer, apr_size_t *len) ($*2_type temp) {
    if (!PyInt_Check($input)) {
        PyErr_SetString(PyExc_TypeError,
                        "expecting an integer for the buffer size");
        SWIG_fail;
    }
    temp = PyInt_AsLong($input);
    if (temp < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "buffer size must be a positive integer");
        SWIG_fail;
    }
    $1 = malloc(temp);
    $2 = ($2_ltype)&temp;
}
#endif
#ifdef SWIGPERL
%typemap(in) (char *buffer, apr_size_t *len) ($*2_type temp) {
    temp = SvIV($input);
    $1 = malloc(temp);
    $2 = ($2_ltype)&temp;
}
#endif
#ifdef SWIGRUBY
%typemap(in) (char *buffer, apr_size_t *len) ($*2_type temp) {
    temp = NUM2LONG($input);
    $1 = malloc(temp);
    $2 = ($2_ltype)&temp;
}
#endif

/* ### need to use freearg or somesuch to ensure the string is freed.
   ### watch out for 'return' anywhere in the binding code. */

#ifdef SWIGPYTHON
%typemap(argout) (char *buffer, apr_size_t *len) {
  %append_output(PyString_FromStringAndSize($1, *$2));
  free($1);
}
#endif
#ifdef SWIGPERL
%typemap(argout) (char *buffer, apr_size_t *len) {
  %append_output(sv_2mortal(newSVpvn($1, *$2)));
  free($1);
}
#endif
#ifdef SWIGRUBY
%typemap(argout) (char *buffer, apr_size_t *len) {
  %append_output(*$2 == 0 ? Qnil : rb_str_new($1, *$2));
  free($1);
}
#endif

/* -----------------------------------------------------------------------
   fix up the svn_stream_write() ptr/len arguments
*/
#ifdef SWIGPYTHON
%typemap(in) (const char *data, apr_size_t *len) ($*2_type temp) {
    if (!PyString_Check($input)) {
        PyErr_SetString(PyExc_TypeError,
                        "expecting a string for the buffer");
        SWIG_fail;
    }
    $1 = PyString_AS_STRING($input);
    temp = PyString_GET_SIZE($input);
    $2 = ($2_ltype)&temp;
}
#endif
#ifdef SWIGPERL
%typemap(in) (const char *data, apr_size_t *len) ($*2_type temp) {
    $1 = SvPV($input, temp);
    $2 = ($2_ltype)&temp;
}
#endif
#ifdef SWIGRUBY
%typemap(in) (const char *data, apr_size_t *len) ($*2_type temp)
{
  $1 = StringValuePtr($input);
  temp = RSTRING($input)->len;
  $2 = ($2_ltype)&temp;
}
#endif

#ifdef SWIGPYTHON
%typemap(argout) (const char *data, apr_size_t *len) {
  %append_output(PyInt_FromLong(*$2));
}
#endif

#ifdef SWIGPERL
%typemap(argout) (const char *data, apr_size_t *len) {
  %append_output(sv_2mortal(newSViv(*$2)));
}
#endif

#ifdef SWIGRUBY
%typemap(argout) (const char *data, apr_size_t *len) {
  %append_output(LONG2NUM(*$2));
}
#endif

/* -----------------------------------------------------------------------
   auth parameter set/get
*/

/* set */
#ifdef SWIGPYTHON
%typemap(in) const void *value {
    if (PyString_Check($input)) {
        $1 = (void *)PyString_AS_STRING($input);
    }
    else if (PyLong_Check($input)) {
        $1 = (void *)PyLong_AsLong($input);
    }
    else if (PyInt_Check($input)) {
        $1 = (void *)PyInt_AsLong($input);
    }
    else {
        PyErr_SetString(PyExc_TypeError, "not a known type");
        SWIG_fail;
    }
}
#endif

/*
  - all values are converted to char*
  - assume the first argument is Ruby object for svn_auth_baton_t*
*/
#ifdef SWIGRUBY
%typemap(in) const void *value
{
  if (NIL_P($input)) {
    $1 = (void *)NULL;
  } else {
    VALUE _rb_pool;
    apr_pool_t *_global_pool;
    char *value = StringValuePtr($input);

    svn_swig_rb_get_pool(1, argv, Qnil, &_rb_pool, &_global_pool);
    $1 = (void *)apr_pstrdup(_global_pool, value);
  }
}
#endif

/* get */
/* assume the value is char* */
#ifdef SWIGRUBY
%typemap(out) const void *
{
  char *value = $1;
  if (value) {
    $result = rb_str_new2(value);
  } else {
    $result = Qnil;
  }
}
#endif

#ifndef SWIGRUBY
%ignore svn_auth_get_parameter;
#endif

/* -----------------------------------------------------------------------
   svn_mergeinfo_parse()
*/

%apply apr_hash_t **MERGEHASH {
    apr_hash_t **mergehash,
    apr_hash_t **deleted,
    apr_hash_t **added,
    apr_hash_t **mergeoutput
}

/* -----------------------------------------------------------------------
   svn_io_parse_mimetypes_file()
*/

#ifdef SWIGRUBY
%apply apr_hash_t **HASH_CSTRING {
    apr_hash_t **type_map
}
#endif

/* -----------------------------------------------------------------------
   svn_io_detect_mimetype2()
*/

%apply apr_hash_t *HASH_CSTRING {
    apr_hash_t *mimetype_map
}

/* -----------------------------------------------------------------------
   describe how to pass a FILE* as a parameter (svn_stream_from_stdio)
*/
#ifdef SWIGPYTHON
%typemap(in) FILE * {
    $1 = PyFile_AsFile($input);
    if ($1 == NULL) {
        PyErr_SetString(PyExc_ValueError, "Must pass in a valid file object");
        SWIG_fail;
    }
}
#endif
#ifdef SWIGPERL
%typemap(in) FILE * {
    $1 = PerlIO_exportFILE (IoIFP (sv_2io ($input)), NULL);
}
#endif

/* -----------------------------------------------------------------------
   wrap some specific APR functionality
*/

apr_status_t apr_initialize(void);
void apr_terminate(void);

apr_status_t apr_time_ansi_put(apr_time_t *result, time_t input);

void apr_pool_destroy(apr_pool_t *p);
void apr_pool_clear(apr_pool_t *p);

apr_status_t apr_file_open_stdout (apr_file_t **out, apr_pool_t *pool);
apr_status_t apr_file_open_stderr (apr_file_t **out, apr_pool_t *pool);

/* Allow parsing of apr_errno.h without parsing apr.h. */
#define APR_DECLARE(x) x
/* Not wrapped, use svn_strerror instead. */
%ignore apr_strerror;
/* Wrap the APR status and error codes. */
/* Sigh, or not. This would mean actually having access to apr_errno.h at
   wrapper generation time, which, when rolling tarballs, the include paths
   are not currently set up to give us. FIXME. So, instead, we replicate
   one important typedef here instead.
%include apr_errno.h
*/
typedef int apr_status_t;

/* -----------------------------------------------------------------------
   pool functions renaming since swig doesn't take care of the #define's
*/
%rename (svn_pool_create) svn_pool_create_ex;
%ignore svn_pool_create_ex_debug;
%typemap(default) apr_allocator_t *allocator {
    $1 = NULL;
}

/* -----------------------------------------------------------------------
   Default pool handling for perl.
*/
#ifdef SWIGPERL
apr_pool_t *current_pool;

#if SWIG_VERSION <= 0x010324
%{
#define SVN_SWIGEXPORT(t) SWIGEXPORT(t)
%}
#else
%{
#define SVN_SWIGEXPORT(t) SWIGEXPORT t
%}
#endif

%{

static apr_pool_t *current_pool = 0;

SVN_SWIGEXPORT(apr_pool_t *)
svn_swig_pl_get_current_pool (void)
{
  return current_pool;
}

SVN_SWIGEXPORT(void)
svn_swig_pl_set_current_pool (apr_pool_t *pool)
{
  current_pool = pool;
}

%}

#endif

/* -----------------------------------------------------------------------
   wrap config functions
*/

#ifdef SWIGPERL
%callback_typemap(svn_config_enumerator_t callback, void *baton,
                  ,
                  svn_swig_pl_thunk_config_enumerator,
                  )
#endif

#ifdef SWIGRUBY
%callback_typemap(svn_config_enumerator2_t callback, void *baton,
                  ,
                  ,
                  svn_swig_rb_config_enumerator)

%callback_typemap(svn_config_section_enumerator2_t callback, void *baton,
                  ,
                  ,
                  svn_swig_rb_config_section_enumerator)
#endif

/* Allow None to be passed as config_dir argument */
#ifdef SWIGPYTHON
%typemap(in,parse="z") const char *config_dir "";
#endif
#ifdef SWIGRUBY
%typemap(in) const char *config_dir {
  if (NIL_P($input)) {
    $1 = "";
  } else {
    $1 = StringValuePtr($input);
  }
}
#endif

#ifdef SWIGPYTHON
PyObject *svn_swig_py_exception_type(void);
#endif

/* -----------------------------------------------------------------------
  thunk the various authentication prompt functions.
  PERL NOTE: store the inputed SV in _global_callback for use in the
             later argout typemap
*/
#ifdef SWIGPERL
%define %authprompt_callback_typemap(AuthType)
%typemap(in) (svn_auth_ ## AuthType ## _prompt_func_t prompt_func,
              void *prompt_baton) {
  $1 = svn_swig_pl_thunk_ ## AuthType ## _prompt;
  $2 = $input;
  _global_callback = $input;
}
%enddef
#else
%define %authprompt_callback_typemap(AuthType)
%callback_typemap(svn_auth_ ## AuthType ## _prompt_func_t prompt_func,
                  void *prompt_baton,
                  svn_swig_py_auth_ ## AuthType ## _prompt_func,,
                  svn_swig_rb_auth_ ## AuthType ## _prompt_func)
%enddef
#endif

%authprompt_callback_typemap(simple)
%authprompt_callback_typemap(username)
%authprompt_callback_typemap(ssl_server_trust)
%authprompt_callback_typemap(ssl_client_cert)
%authprompt_callback_typemap(ssl_client_cert_pw)

/* -----------------------------------------------------------------------
 * For all the various functions that set a callback baton create a reference
 * for the baton (which in this case is an SV pointing to the callback)
 * and make that a return from the function.  The perl side should
 * then store the return in the object the baton is attached to.
 * If the function already returns a value then this value is follows that
 * function.  In the case of the prompt functions auth_open_helper in Core.pm
 * is used to split up these values.
*/
#ifdef SWIGPERL
%typemap(argout) void *CALLBACK_BATON (SV * _global_callback) {
  /* callback baton */
  %append_output(sv_2mortal(newRV_inc(_global_callback)));
}

%typemap(in) void *CALLBACK_BATON (SV * _global_callback) {
  _global_callback = $input;
  $1 = (void *) _global_callback;
}

%apply void *CALLBACK_BATON {
  void *prompt_baton
};
#endif


/* ----------------------------------------------------------------------- */
#ifdef SWIGRUBY
%ignore svn_auth_open;
%ignore svn_diff_file_options_create;
%ignore svn_create_commit_info;
%ignore svn_commit_info_dup;
#endif

/* ----------------------------------------------------------------------- */

%include svn_error_codes_h.swg
%include svn_time_h.swg
%include svn_types_h.swg
%include svn_pools_h.swg
%include svn_version_h.swg

/* The constant SVN_PROP_REVISION_ALL_PROPS is a C fragment, not a single
   data value, so the SWIG parser will raise a 305 warning if we don't
   suppress it. */
#pragma SWIG nowarn=305
%include svn_props_h.swg
#pragma SWIG nowarn=+305

%include svn_opt_h.swg
%include svn_auth_h.swg
%include svn_config_h.swg
%include svn_utf_h.swg
%include svn_nls_h.swg
%include svn_path_h.swg
%include svn_mergeinfo_h.swg
%include svn_io_h.swg

#ifdef SWIGPERL
%include svn_md5_h.swg
%include svn_diff_h.swg
%include svn_error_h.swg

%{
#include "svn_private_config.h"
%}
%init %{
#if defined(SVN_AVOID_CIRCULAR_LINKAGE_AT_ALL_COSTS_HACK)
  svn_swig_pl_bind_current_pool_fns (&svn_swig_pl_get_current_pool,
                                     &svn_swig_pl_set_current_pool);
#endif
%}
#endif

#ifdef SWIGPYTHON

void svn_swig_py_set_application_pool(PyObject *py_pool, apr_pool_t *pool);
void svn_swig_py_clear_application_pool();

%init %{
/* Theoretically, we should be checking for errors here,
   but I do not know of any useful way to signal an error to Python
   from within a module initialization function. */
svn_swig_py_initialize();

/* This is a hack.  I dunno if we can count on SWIG calling the module "m" */
PyModule_AddObject(m, "SubversionException",
                   svn_swig_py_register_exception());
%}

%pythoncode %{
SubversionException = _core.SubversionException
%}

/* Proxy classes for APR classes */
%include proxy_apr.swg

#endif

#ifdef SWIGRUBY
%init %{
  svn_swig_rb_initialize();
%}

%header %{
static void apr_pool_wrapper_destroy(apr_pool_wrapper_t *self);
static void apr_pool_wrapper_destroy_children(apr_pool_wrapper_t *self);
static void apr_pool_wrapper_remove_from_parent(apr_pool_wrapper_t *self);
%}

/* Dummy declaration */
struct apr_pool_wrapper_t
{
};

/* Leave memory administration to ruby's GC */
%extend apr_pool_wrapper_t
{
  apr_pool_wrapper_t(apr_pool_wrapper_t *parent) {
    apr_pool_wrapper_t *self;
    apr_pool_t *parent_pool;

    self = ALLOC(apr_pool_wrapper_t);
    if (parent) {
      parent_pool = parent->pool;
      APR_ARRAY_PUSH(parent->children, apr_pool_wrapper_t *) = self;
    } else {
      parent_pool = NULL;
    }
    self->pool = svn_pool_create_ex(parent_pool, NULL);
    self->destroyed = FALSE;
    self->parent = parent;
    self->children = apr_array_make(self->pool, 0,
                                    sizeof(apr_pool_wrapper_t *));
    return self;
  }

  ~apr_pool_wrapper_t() {
    apr_pool_wrapper_destroy(self);
    xfree(self);
  }

  void _destroy(void) {
    apr_pool_wrapper_destroy(self);
  }
};

%ignore apr_pool_wrapper_destroy;
%ignore apr_pool_wrapper_destroy_children;
%ignore apr_pool_wrapper_remove_from_parent;
%inline %{
static void
apr_pool_wrapper_destroy(apr_pool_wrapper_t *self)
{
  if (!self->destroyed) {
    self->destroyed = TRUE;
    apr_pool_wrapper_destroy_children(self);
    apr_pool_wrapper_remove_from_parent(self);
    apr_pool_destroy(self->pool);
  }
}

static void
apr_pool_wrapper_destroy_children(apr_pool_wrapper_t *self)
{
  apr_pool_wrapper_t **child;

  while ((child = apr_array_pop(self->children))) {
    if (*child) {
      apr_pool_wrapper_destroy(*child);
    }
  }
}

static void
apr_pool_wrapper_remove_from_parent(apr_pool_wrapper_t *self)
{
  if (self->parent) {
    apr_pool_wrapper_t *child;
    int i, len;

    len = self->parent->children->nelts;
    for (i = 0; i < len; i++) {
      child = APR_ARRAY_IDX(self->parent->children, i, apr_pool_wrapper_t *);
      if (child == self) {
        APR_ARRAY_IDX(self->parent->children, i, apr_pool_wrapper_t *) = NULL;
        self->parent = NULL;
        break;
      }
    }
  }
}
%}

/* Dummy declaration */
struct svn_stream_t
{
};

%extend svn_stream_t
{
  svn_stream_t(VALUE io) {
    return svn_swig_rb_make_stream(io);
  };

  ~svn_stream_t() {
  };
}

/* Dummy declaration */
struct svn_auth_baton_t
{
};

%extend svn_auth_baton_t
{
  svn_auth_baton_t(apr_array_header_t *providers, apr_pool_t *pool) {
    svn_auth_baton_t *self;
    svn_auth_open(&self, providers, pool);
    return self;
  };

  ~svn_auth_baton_t() {
  };
}

%extend svn_diff_file_options_t
{
  svn_diff_file_options_t(apr_pool_t *pool) {
    return svn_diff_file_options_create(pool);
  };

  ~svn_diff_file_options_t() {
  };
}

%extend svn_commit_info_t
{
  svn_commit_info_t(apr_pool_t *pool) {
    return svn_create_commit_info(pool);
  };

  ~svn_commit_info_t() {
  };

  svn_commit_info_t *dup(apr_pool_t *pool) {
    return svn_commit_info_dup(self, pool);
  };
}

%include svn_diff_h.swg

%inline %{
static VALUE
svn_default_charset(void)
{
  return INT2NUM((int)APR_DEFAULT_CHARSET);
}

static VALUE
svn_locale_charset(void)
{
  return INT2NUM((int)APR_LOCALE_CHARSET);
}

/* prompt providers return baton for protecting GC */
static VALUE
svn_swig_rb_auth_get_simple_prompt_provider(
  svn_auth_provider_object_t **provider,
  svn_auth_simple_prompt_func_t prompt_func,
  void *prompt_baton,
  int retry_limit,
  apr_pool_t *pool)
{
  svn_auth_get_simple_prompt_provider(provider, prompt_func, prompt_baton,
                                      retry_limit, pool);
  return rb_ary_new3(1, (VALUE)prompt_baton);
}

static VALUE
svn_swig_rb_auth_get_ssl_client_cert_prompt_provider(
  svn_auth_provider_object_t **provider,
  svn_auth_ssl_client_cert_prompt_func_t prompt_func,
  void *prompt_baton,
  int retry_limit,
  apr_pool_t *pool)
{
  svn_auth_get_ssl_client_cert_prompt_provider(provider, prompt_func,
                                               prompt_baton, retry_limit,
                                               pool);
  return rb_ary_new3(1, (VALUE)prompt_baton);
}

static VALUE
svn_swig_rb_auth_get_ssl_client_cert_pw_prompt_provider(
  svn_auth_provider_object_t **provider,
  svn_auth_ssl_client_cert_pw_prompt_func_t prompt_func,
  void *prompt_baton,
  int retry_limit,
  apr_pool_t *pool)
{
  svn_auth_get_ssl_client_cert_pw_prompt_provider(provider, prompt_func,
                                                  prompt_baton, retry_limit,
                                                  pool);
  return rb_ary_new3(1, (VALUE)prompt_baton);
}

static VALUE
svn_swig_rb_auth_get_ssl_server_trust_prompt_provider(
  svn_auth_provider_object_t **provider,
  svn_auth_ssl_server_trust_prompt_func_t prompt_func,
  void *prompt_baton,
  apr_pool_t *pool)
{
  svn_auth_get_ssl_server_trust_prompt_provider(provider, prompt_func,
                                                prompt_baton, pool);
  return rb_ary_new3(1, (VALUE)prompt_baton);
}

static VALUE
svn_swig_rb_auth_get_username_prompt_provider(
  svn_auth_provider_object_t **provider,
  svn_auth_username_prompt_func_t prompt_func,
  void *prompt_baton,
  int retry_limit,
  apr_pool_t *pool)
{
  svn_auth_get_username_prompt_provider(provider, prompt_func, prompt_baton,
                                        retry_limit, pool);
  return rb_ary_new3(1, (VALUE)prompt_baton);
}
%}
#endif
