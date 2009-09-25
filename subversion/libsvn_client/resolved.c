/*
 * resolved.c:  wrapper around wc resolved functionality.
 *
 * ====================================================================
 *    Licensed to the Subversion Corporation (SVN Corp.) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The SVN Corp. licenses this file
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

#include "svn_types.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "client.h"
#include "private/svn_wc_private.h"


/*** Code. ***/

svn_error_t *
svn_client_resolve(const char *path,
                   svn_depth_t depth,
                   svn_wc_conflict_choice_t conflict_choice,
                   svn_client_ctx_t *ctx,
                   apr_pool_t *pool)
{
  svn_wc_adm_access_t *adm_access;
  int adm_lock_level = SVN_WC__LEVELS_TO_LOCK_FROM_DEPTH(depth);
  svn_boolean_t wc_root;
  const char *local_abspath;

  SVN_ERR(svn_dirent_get_absolute(&local_abspath, path, pool));

  /* Make sure we do not end up looking for tree conflict info
   * above the working copy root. It's OK to check for tree conflict
   * info in the parent of a *switched* subtree, because the
   * subtree itself might be a tree conflict victim. */
  SVN_ERR(svn_wc__strictly_is_wc_root(&wc_root, ctx->wc_ctx, local_abspath,
                                      pool));

  if (! wc_root) /* but possibly a switched subdir */
    {
      /* In order to resolve tree-conflicts on the target PATH, we need an
       * adm_access on its parent directory. The lock level then needs to
       * extend at least onto the immediate children. */
      if (adm_lock_level >= 0)
        adm_lock_level++;
      SVN_ERR(svn_wc__adm_probe_in_context(&adm_access, ctx->wc_ctx,
                                           svn_dirent_dirname(path, pool),
                                           TRUE, adm_lock_level,
                                           ctx->cancel_func,
                                           ctx->cancel_baton,
                                           pool));
    }
  else
    {
      SVN_ERR(svn_wc__adm_probe_in_context(&adm_access, ctx->wc_ctx,
                                           path,
                                           TRUE,
                                           adm_lock_level,
                                           ctx->cancel_func,
                                           ctx->cancel_baton,
                                           pool));
    }

  SVN_ERR(svn_wc_resolved_conflict5(ctx->wc_ctx, local_abspath,
                                    depth, TRUE, "", TRUE,
                                    conflict_choice,
                                    ctx->cancel_func, ctx->cancel_baton,
                                    ctx->notify_func2, ctx->notify_baton2,
                                    pool));

  return svn_wc_adm_close2(adm_access, pool);
}
