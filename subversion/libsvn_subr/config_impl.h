/*
 * config_impl.h :  private header for the config file implementation.
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



#ifndef SVN_CONFIG_IMPL_H
#define SVN_CONFIG_IMPL_H


#include <apr_hash.h>
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_string.h"
#include "svn_config.h"
#include "svn_private_config.h"


/* The configuration data. This is a superhash of sections and options. */
struct svn_config_t
{
  /* Table of cfg_section_t's. */
  apr_hash_t *sections;

  /* Pool for hash tables, table entries and unexpanded values */
  apr_pool_t *pool;

  /* Pool for expanded values -- this is separate, so that we can
     clear it when modifying the config data. */
  apr_pool_t *x_pool;

  /* Indicates that some values in the configuration have been expanded. */
  svn_boolean_t x_values;

  /* Temporary string used for lookups. */
  svn_stringbuf_t *tmp_key;
};


/* Read sectinos and options from a file. */
svn_error_t *svn_config__parse_file (svn_config_t *cfg,
                                     const char *file,
                                     svn_boolean_t must_exist);


#ifdef SVN_WIN32
/* Read sectinos and options from the Windows Registry. */
svn_error_t *svn_config__parse_registry (svn_config_t *cfg,
                                         const char *file,
                                         svn_boolean_t must_exist);

#  define SVN_REGISTRY_PREFIX "REGISTRY:"
#  define SVN_REGISTRY_PREFIX_LEN (9)
#  define SVN_REGISTRY_HKLM "HKLM\\"
#  define SVN_REGISTRY_HKLM_LEN (5)
#  define SVN_REGISTRY_HKCU "HKCU\\"
#  define SVN_REGISTRY_HKCU_LEN (5)
#endif /* SVN_WIN32 */

#endif /* SVN_CONFIG_IMPL_H */


/*
 * local variables:
 * eval: (load-file "../svn-dev.el")
 * end:
 */
