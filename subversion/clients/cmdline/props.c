/*
 * props.c: Utility functions for property handling
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

/* ==================================================================== */



/*** Includes. ***/

#include <apr_hash.h>
#include "svn_cmdline.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_path.h"
#include "svn_delta.h"
#include "svn_error.h"
#include "svn_subst.h"
#include "cl.h"




svn_error_t *
svn_cl__revprop_no_rev_error (apr_pool_t *pool)
{
  return svn_error_create
    (SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
     "Must specify revision explicitly when operating on a "
     "revision property.");
}


svn_error_t *
svn_cl__print_prop_hash (apr_hash_t *prop_hash,
                         svn_boolean_t names_only,
                         apr_pool_t *pool)
{
  apr_hash_index_t *hi;

  for (hi = apr_hash_first (pool, prop_hash); hi; hi = apr_hash_next (hi))
    {
      const void *key;
      void *val;
      const char *pname;
      svn_string_t *propval;
      const char *pname_stdout;

      apr_hash_this (hi, &key, NULL, &val);
      pname = key;
      propval = val;

      if (svn_prop_needs_translation (pname))
        SVN_ERR (svn_subst_detranslate_string (&propval, propval,
                                               TRUE, pool));

      SVN_ERR (svn_cmdline_cstring_from_utf8 (&pname_stdout, pname, pool));

      if (names_only)
        printf ("  %s\n", pname_stdout);
      else
        printf ("  %s : %s\n", pname_stdout, propval->data);
    }

  return SVN_NO_ERROR;
}
