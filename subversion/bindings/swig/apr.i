/*
 * apr.i :  SWIG interface file for selected APR types
 *
 * ====================================================================
 * Copyright (c) 2000-2003 CollabNet.  All rights reserved.
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

/* This is the interface for the APR headers. This is not built as a module
   because we aren't going to wrap the APR functions. Thus, we only define
   the various types in here, as necessary.

   Actually, util.i wraps a few, key functions.
*/

%include typemaps.i


/* ----------------------------------------------------------------------- */

/* define an OUTPUT typemap for 'apr_off_t *'. for now, we'll treat it as
   a 'long' even if that isn't entirely correct... */
%typemap(in,numinputs=0) apr_off_t * (apr_off_t temp)
    "$1 = &temp;";

%typemap(python,argout,fragment="t_output_helper") apr_off_t *
    "$result = t_output_helper($result,PyInt_FromLong((long) (*$1)));";

%typemap(java,argout) apr_off_t * {
    /* ### FIXME */
}

/* ----------------------------------------------------------------------- */

%include apr.h

/* ### be nice to have all the error values and macros. there are some
   ### problems including this file, tho. SWIG isn't smart enough with some
   ### of the preprocessing and thinks there is a macro redefinition */
//%include apr_errno.h
typedef int apr_status_t;

/* ### seems that SWIG isn't picking up the definition of size_t */
typedef unsigned long size_t;

/* Define the time type (rather than picking up all of apr_time.h) */
typedef apr_int64_t apr_time_t;

typedef apr_int32_t time_t;

/* -----------------------------------------------------------------------
   handle the mappings for apr_time_t

   Note: we don't generalize this to 'long long' since SWIG is starting
   to handle that.
*/

%apply long long { apr_time_t };

/* 'apr_time_t *' will always be an OUTPUT parameter */
%typemap(in,numinputs=0) apr_time_t * (apr_time_t temp)
    "$1 = &temp;";

%typemap(python,argout,fragment="t_output_helper") apr_time_t *
    "$result = t_output_helper($result, PyLong_FromLongLong(*$1));";

%typemap(java,argout) apr_time_t * {
    jclass cls = JCALL1(FindClass, jenv, "java/lang/Long");
    jmethodID ctor = JCALL3(GetMethodID, jenv, cls, "<init>", "(J)V");
    jobject l = JCALL3(NewObject, jenv, cls, ctor, (jlong) *$1);
    $result = t_output_helper($result, JCALL1(NewGlobalRef, jenv, l));
}

/* -----------------------------------------------------------------------
   create some INOUT typemaps for apr_size_t
*/

%apply unsigned long *INOUT { apr_size_t *INOUT };

%typemap(python,in) apr_size_t *INOUT (apr_size_t temp) {
    temp = (apr_size_t) PyInt_AsLong($input);
    $1 = &temp;
}
%typemap(java,in) apr_size_t *INOUT (apr_size_t temp) {
    jclass cls = JCALL1(FindClass, jenv, "java/lang/Long");
    jmethodID mid = JCALL3(GetStaticMethodID, jenv, cls, "longValue", "()J");
    temp = (apr_size_t) JCALL2(CallStaticLongMethod, jenv, mid, $input);
    $1 = &temp;
}

/* -----------------------------------------------------------------------
   create an OUTPUT argument typemap for an apr_hash_t **
*/

%typemap(in,numinputs=0) apr_hash_t **OUTPUT (apr_hash_t *temp)
    "$1 = &temp;";

/* -----------------------------------------------------------------------
   create an OUTPUT argument defn for an apr_hash_t ** which is storing
   property values
*/

%typemap(in,numinputs=0) apr_hash_t **PROPHASH = apr_hash_t **OUTPUT;
%typemap(python,argout) apr_hash_t **PROPHASH {
    /* toss prior result, get new result from the hash */
    Py_DECREF($result);
    $result = svn_swig_py_prophash_to_dict(*$1);
}
%typemap(java,argout) apr_hash_t **PROPHASH {
    /* toss prior result, get new result from the hash */
    JCALL1(DeleteGlobalRef, jenv, $result);
    $result = JCALL1(NewGlobalRef, jenv,
                     svn_swig_java_prophash_to_dict(jenv, *$1));
}

/* -----------------------------------------------------------------------
  handle apr_file_t *
*/

%typemap(python, in) apr_file_t * {
  $1 = svn_swig_py_make_file($input, _global_pool);
}

/* -----------------------------------------------------------------------
   apr_file_t ** is always an OUT param
*/

%typemap(in,numinputs=0) apr_file_t ** (apr_file_t *temp)
    "$1 = &temp;";

%typemap(python,argout,fragment="t_output_helper") apr_file_t **
    "$result = t_output_helper(
        $result,
        SWIG_NewPointerObj(*$1, $*1_descriptor, 0));";
%typemap(java,argout,fragment="t_output_helper") apr_file_t **
    /* HELP: Is there a JNI equivalent of SWIG_NewPointerObj, or is
       this actually a cross-language typemap? */
    "$result = t_output_helper(
        $result,
        SWIG_NewPointerObj(*$1, $*1_descriptor, 0));";

/* ----------------------------------------------------------------------- */
