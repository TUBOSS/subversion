/*
 * changelist-cmd.c -- Associate (or deassociate) a wc path with a changelist.
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

#include "svn_client.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_utf.h"

#include "cl.h"

#include "svn_private_config.h"




/* This implements the `svn_opt_subcommand_t' interface. */
svn_error_t *
svn_cl__changelist(apr_getopt_t *os,
                   void *baton,
                   apr_pool_t *pool)
{
  const char *changelist_name = NULL;
  svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
  svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
  apr_array_header_t *targets;
  svn_depth_t depth = opt_state->depth;
  int i;

  /* If we're not removing changelists, then our first argument should
     be the name of a changelist. */

  if (! opt_state->remove)
    {
      apr_array_header_t *args;
      SVN_ERR(svn_opt_parse_num_args(&args, os, 1, pool));
      changelist_name = APR_ARRAY_IDX(args, 0, const char *);
      SVN_ERR(svn_utf_cstring_to_utf8(&changelist_name,
                                      changelist_name, pool));
    }

  /* Parse the remaining arguments as paths. */
  SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
                                                      opt_state->targets,
                                                      ctx, FALSE, pool));

  /* Changelist has no implicit dot-target `.', so don't you put that
     code here! */
  if (! targets->nelts)
    return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);

  /* Don't even attempt to modify the working copy if any of the
   * targets look like URLs. URLs are invalid input. */
  for (i = 0; i < targets->nelts; i++)
    {
      const char *target = APR_ARRAY_IDX(targets, i, const char *);

      if (svn_path_is_url(target))
        return svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                 _("'%s' is not a local path"), target);
    }

  if (opt_state->quiet)
    /* FIXME: This is required because svn_client_create_context()
       always initializes ctx->notify_func2 to a wrapper function
       which calls ctx->notify_func() if it isn't NULL.  In other
       words, typically, ctx->notify_func2 is never NULL.  This isn't
       usually a problem, but the changelist logic generates
       svn_error_t's as part of its notification.

       So, svn_wc_set_changelist() checks its notify_func (our
       ctx->notify_func2) for NULL-ness, and seeing non-NULL-ness,
       generates a notificaton object and svn_error_t to describe some
       problem.  It passes that off to its notify_func (our
       ctx->notify_func2) which drops the notification on the floor
       (because it wraps a NULL ctx->notify_func).  But svn_error_t's
       dropped on the floor cause SEGFAULTs at pool cleanup time --
       they need instead to be cleared.

       SOOOooo... we set our ctx->notify_func2 to NULL so the WC code
       doesn't even generate the errors.  */
    ctx->notify_func2 = NULL;

  if (depth == svn_depth_unknown)
    depth = svn_depth_empty;

  SVN_ERR(svn_cl__eat_peg_revisions(&targets, targets, pool));

  if (changelist_name)
    {
      return svn_cl__try
              (svn_client_add_to_changelist(targets, changelist_name,
                                            depth, opt_state->changelists,
                                            ctx, pool),
               NULL, opt_state->quiet,
               SVN_ERR_UNVERSIONED_RESOURCE,
               SVN_ERR_WC_PATH_NOT_FOUND,
               SVN_NO_ERROR);
    }
  else
    {
      return svn_cl__try
              (svn_client_remove_from_changelists(targets, depth,
                                                  opt_state->changelists,
                                                  ctx, pool),
               NULL, opt_state->quiet,
               SVN_ERR_UNVERSIONED_RESOURCE,
               SVN_ERR_WC_PATH_NOT_FOUND,
               SVN_NO_ERROR);
    }
}
