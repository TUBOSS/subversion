/*
 * svn_repos.i :  SWIG interface file for svn_repos.h
 *
 * ====================================================================
 * Copyright (c) 2000-2001 CollabNet.  All rights reserved.
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

%module _repos

%import apr.i
%import svn_types.i
%import svn_string.i
%import svn_fs.i
/* -----------------------------------------------------------------------
   these types (as 'type **') will always be an OUT param
*/
OUT_PARAM(svn_repos_t);

/* -----------------------------------------------------------------------
   handle the 'paths' parameter appropriately
*/
%typemap(in) const apr_array_header_t *paths =
    const apr_array_header_t *STRINGLIST;

/* ----------------------------------------------------------------------- */

%include svn_repos.h
%{
#include "svn_repos.h"
%}
