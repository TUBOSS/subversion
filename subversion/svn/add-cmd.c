/*
 * add-cmd.c -- Subversion add/unadd commands
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

/* ==================================================================== */



/*** Includes. ***/
#define APR_WANT_STDIO
#include <apr_want.h>

#include "svn_path.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "cl.h"

#include "svn_private_config.h"


/*** Code. ***/

/* This implements the `svn_opt_subcommand_t' interface. */
svn_error_t *
svn_cl__add(apr_getopt_t *os,
            void *baton,
            apr_pool_t *pool)
{
  svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
  svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
  apr_array_header_t *targets;
  int i;
  apr_pool_t *iterpool;

  SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
                                                      opt_state->targets,
                                                      ctx, pool));

  if (! targets->nelts)
    return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);

  if (! opt_state->quiet)
    SVN_ERR(svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2,
                                 FALSE, pool));

  if (opt_state->depth == svn_depth_unknown)
    opt_state->depth = svn_depth_infinity;

  SVN_ERR(svn_cl__eat_peg_revisions(&targets, targets, pool));

  /* Don't even attempt to modify the working copy if any of the
   * targets look like URLs. URLs are invalid input. */
  for (i = 0; i < targets->nelts; i++)
    {
      const char *target = APR_ARRAY_IDX(targets, i, const char *);

      if (svn_path_is_url(target))
        return svn_error_return(svn_error_createf(SVN_ERR_ILLEGAL_TARGET,
                                                  NULL,
                                                  _("'%s' is not a local path"),
                                                  target));
    }

  iterpool = svn_pool_create(pool);
  for (i = 0; i < targets->nelts; i++)
    {
      const char *target = APR_ARRAY_IDX(targets, i, const char *);

      svn_pool_clear(iterpool);
      SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
      SVN_ERR(svn_cl__try
              (svn_client_add4(svn_dirent_canonicalize(target, iterpool),
                               opt_state->depth,
                               opt_state->force, opt_state->no_ignore,
                               opt_state->parents, ctx, iterpool),
               NULL, opt_state->quiet,
               SVN_ERR_ENTRY_EXISTS,
               SVN_ERR_WC_PATH_NOT_FOUND,
               SVN_NO_ERROR));
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}
