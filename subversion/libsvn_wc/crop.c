/*
 * crop.c: Cropping the WC
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

#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"

#include "wc.h"
#include "workqueue.h"

#include "svn_private_config.h"

/* A baton for use with modcheck_found_entry(). */
typedef struct modcheck_baton_t {
  svn_wc__db_t *db;         /* wc_db to access nodes */
  svn_boolean_t found_mod;  /* whether a modification has been found */
  svn_boolean_t found_not_delete;  /* Found a not-delete modification */
} modcheck_baton_t;

/* An implementation of svn_wc_status_func4_t, similar to a function
   with the same name in update_editor.c, but with slightly different
   behavior around unversioned items */
static svn_error_t *
modcheck_callback(void *baton,
                  const char *local_abspath,
                  const svn_wc_status3_t *status,
                  apr_pool_t *scratch_pool)
{
  modcheck_baton_t *mb = baton;

  switch (status->node_status)
    {
      case svn_wc_status_normal:
      case svn_wc_status_incomplete:
      case svn_wc_status_ignored:
      case svn_wc_status_none:
      case svn_wc_status_external:
        break;

      case svn_wc_status_deleted:
        mb->found_mod = TRUE;
        break;

      case svn_wc_status_missing:
      case svn_wc_status_obstructed:
        mb->found_mod = TRUE;
        mb->found_not_delete = TRUE;
        /* Exit from the status walker: We know what we want to know */
        return svn_error_create(SVN_ERR_CEASE_INVOCATION, NULL, NULL);

      default:
      case svn_wc_status_added:
      case svn_wc_status_replaced:
      case svn_wc_status_modified:
      case svn_wc_status_unversioned:
        mb->found_mod = TRUE;
        mb->found_not_delete = TRUE;
        /* Exit from the status walker: We know what we want to know */
        return svn_error_create(SVN_ERR_CEASE_INVOCATION, NULL, NULL);
    }

  return SVN_NO_ERROR;
}


/* Set *ALLOW to true if the path can be safely deleted by the crop operation,
   otherwise to false. */
static svn_error_t *
allow_crop(svn_boolean_t *allow,
           svn_wc__db_t *db,
           const char *local_abspath,
           svn_cancel_func_t cancel_func,
           void *cancel_baton,
           apr_pool_t *scratch_pool)
{
  modcheck_baton_t modcheck_baton = { NULL, FALSE, FALSE };
  svn_error_t *err;

  modcheck_baton.db = db;

  /* Walk the WC tree for status with depth infinity, looking for any local
   * modifications. If it's a "sparse" directory, that's OK: there can be
   * no local mods in the pieces that aren't present in the WC. */

  err = svn_wc__internal_walk_status(db, local_abspath,
                                     svn_depth_infinity,
                                     FALSE, FALSE, FALSE, NULL,
                                     modcheck_callback, &modcheck_baton,
                                     cancel_func, cancel_baton,
                                     scratch_pool);

  if (err && err->apr_err == SVN_ERR_CEASE_INVOCATION)
    svn_error_clear(err);
  else
    SVN_ERR(err);

  *allow = !modcheck_baton.found_mod || (modcheck_baton.found_mod
                                         && !modcheck_baton.found_not_delete);

  return SVN_NO_ERROR;
}


/* Helper function that crops the children of the LOCAL_ABSPATH, under the
 * constraint of NEW_DEPTH. The DIR_PATH itself will never be cropped. The
 * whole subtree should have been locked.
 *
 * DIR_DEPTH is the current depth of LOCAL_ABSPATH as stored in DB.
 *
 * If NOTIFY_FUNC is not null, each file and ROOT of subtree will be reported
 * upon remove.
 */
static svn_error_t *
crop_children(svn_wc__db_t *db,
              const char *local_abspath,
              svn_depth_t dir_depth,
              svn_depth_t new_depth,
              svn_wc_notify_func2_t notify_func,
              void *notify_baton,
              svn_cancel_func_t cancel_func,
              void *cancel_baton,
              apr_pool_t *scratch_pool)
{
  const apr_array_header_t *children;
  apr_pool_t *iterpool;
  int i;

  SVN_ERR_ASSERT(new_depth >= svn_depth_empty
                 && new_depth <= svn_depth_infinity);

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  iterpool = svn_pool_create(scratch_pool);

  if (dir_depth == svn_depth_unknown)
    dir_depth = svn_depth_infinity;

  /* Update the depth of target first, if needed. */
  if (dir_depth > new_depth)
    SVN_ERR(svn_wc__db_op_set_base_depth(db, local_abspath, new_depth,
                                         iterpool));

  /* Looping over current directory's SVN entries: */
  SVN_ERR(svn_wc__db_base_get_children(&children, db, local_abspath,
                                       scratch_pool, iterpool));

  for (i = 0; i < children->nelts; i++)
    {
      const char *child_name = APR_ARRAY_IDX(children, i, const char *);
      const char *child_abspath;
      svn_wc__db_status_t child_status;
      svn_node_kind_t kind;
      svn_depth_t child_depth;
      svn_boolean_t have_work;
      svn_depth_t remove_below;

      svn_pool_clear(iterpool);

      /* Get the next node */
      child_abspath = svn_dirent_join(local_abspath, child_name, iterpool);

      SVN_ERR(svn_wc__db_read_info(&child_status, &kind, NULL, NULL, NULL,
                                   NULL,NULL, NULL, NULL, &child_depth,
                                   NULL, NULL, NULL, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL,
                                   NULL, NULL, NULL, NULL, &have_work,
                                   db, child_abspath, iterpool, iterpool));

      if (have_work)
        {
          if (child_status != svn_wc__db_status_deleted)
            continue; /* Leave local additions alone */

          svn_boolean_t allow;

          SVN_ERR(allow_crop(&allow,
                             db, child_abspath,
                             cancel_func, cancel_baton,
                             iterpool));

          if (!allow)
            continue; /* Something interesting is still there */
        }

      remove_below = (kind == svn_node_dir)
                       ? svn_depth_immediates
                       : svn_depth_files;

      if ((child_status == svn_wc__db_status_server_excluded ||
           child_status == svn_wc__db_status_excluded ||
           child_status == svn_wc__db_status_not_present))
        {
          if (new_depth < remove_below)
            SVN_ERR(svn_wc__db_base_remove(db, child_abspath,
                                           FALSE /* keep_as_working */,
                                           FALSE /* queue_deletes */,
                                           FALSE, FALSE,
                                           SVN_INVALID_REVNUM,
                                           NULL, NULL, iterpool));

          continue; /* No recurse */
        }

      if (new_depth < remove_below)
        {
          svn_boolean_t allow;

          SVN_ERR(allow_crop(&allow,
                             db, child_abspath,
                             cancel_func, cancel_baton,
                             iterpool));

          if (allow)
            {
              SVN_ERR(svn_wc__db_base_remove(db, child_abspath,
                                             FALSE, TRUE, FALSE, FALSE,
                                             SVN_INVALID_REVNUM,
                                             NULL, NULL, iterpool));
              if (notify_func)
                {
                  svn_wc_notify_t *notify;
                  notify = svn_wc_create_notify(child_abspath,
                                                svn_wc_notify_delete,
                                                iterpool);
                  (*notify_func)(notify_baton, notify, iterpool);
                }

              continue; /* No recurse */
            }

          /* Fall through: recurse:*/
        }

      if (kind == svn_node_dir)
        {
          SVN_ERR(crop_children(db, child_abspath,
                                child_depth, svn_depth_empty,
                                notify_func, notify_baton,
                                cancel_func, cancel_baton,
                                iterpool));
        }
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc_exclude(svn_wc_context_t *wc_ctx,
               const char *local_abspath,
               svn_cancel_func_t cancel_func,
               void *cancel_baton,
               svn_wc_notify_func2_t notify_func,
               void *notify_baton,
               apr_pool_t *scratch_pool)
{
  svn_boolean_t is_root, is_switched;
  svn_wc__db_status_t status;
  svn_node_kind_t kind;
  svn_revnum_t revision;
  svn_depth_t depth;
  svn_boolean_t allow;
  const char *repos_relpath, *repos_root, *repos_uuid;

  SVN_ERR(svn_wc__db_is_switched(&is_root, &is_switched, NULL,
                                 wc_ctx->db, local_abspath, scratch_pool));

  if (is_root)
    {
       return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                _("Cannot exclude '%s': "
                                  "it is a working copy root"),
                                svn_dirent_local_style(local_abspath,
                                                       scratch_pool));
    }
  if (is_switched)
    {
      return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                               _("Cannot exclude '%s': "
                                 "it is a switched path"),
                               svn_dirent_local_style(local_abspath,
                                                      scratch_pool));
    }

  SVN_ERR(svn_wc__db_read_info(&status, &kind, &revision, &repos_relpath,
                               &repos_root, &repos_uuid, NULL, NULL, NULL,
                               &depth, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL,
                               wc_ctx->db, local_abspath,
                               scratch_pool, scratch_pool));

  switch (status)
    {
      case svn_wc__db_status_server_excluded:
      case svn_wc__db_status_excluded:
      case svn_wc__db_status_not_present:
        return svn_error_createf(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
                                 _("The node '%s' was not found."),
                                 svn_dirent_local_style(local_abspath,
                                                        scratch_pool));

      case svn_wc__db_status_added:
        /* Would have to check parents if we want to allow this */
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                 _("Cannot exclude '%s': it is to be added "
                                   "to the repository. Try commit instead"),
                                 svn_dirent_local_style(local_abspath,
                                                        scratch_pool));
      case svn_wc__db_status_deleted:
        /* Would have to check parents if we want to allow this */
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                 _("Cannot exclude '%s': it is to be deleted "
                                   "from the repository. Try commit instead"),
                                 svn_dirent_local_style(local_abspath,
                                                        scratch_pool));

      case svn_wc__db_status_normal:
      case svn_wc__db_status_incomplete:
      default:
        break; /* Ok to exclude */
    }

  SVN_ERR(allow_crop(&allow, wc_ctx->db, local_abspath,
                     cancel_func, cancel_baton,
                     scratch_pool));

  if (allow)
    {
      /* Remove all working copy data below local_abspath */
      SVN_ERR(svn_wc__db_base_remove(wc_ctx->db, local_abspath,
                                     FALSE /* keep_working */,
                                     TRUE, FALSE, TRUE,
                                     revision,
                                     NULL, NULL,
                                     scratch_pool));

      SVN_ERR(svn_wc__wq_run(wc_ctx->db, local_abspath,
                             cancel_func, cancel_baton,
                             scratch_pool));

      if (notify_func)
        {
          svn_wc_notify_t *notify;
          notify = svn_wc_create_notify(local_abspath,
                                        svn_wc_notify_exclude,
                                        scratch_pool);
          notify_func(notify_baton, notify, scratch_pool);
        }
    }
  else
    {
      /* Do the next best thing: retry below this path */
      SVN_ERR(crop_children(wc_ctx->db, local_abspath, depth, svn_depth_empty,
                            notify_func, notify_baton,
                            cancel_func, cancel_baton,
                            scratch_pool));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc_crop_tree2(svn_wc_context_t *wc_ctx,
                  const char *local_abspath,
                  svn_depth_t depth,
                  svn_cancel_func_t cancel_func,
                  void *cancel_baton,
                  svn_wc_notify_func2_t notify_func,
                  void *notify_baton,
                  apr_pool_t *scratch_pool)
{
  svn_wc__db_t *db = wc_ctx->db;
  svn_wc__db_status_t status;
  svn_node_kind_t kind;
  svn_depth_t dir_depth;

  /* Only makes sense when the depth is restrictive. */
  if (depth == svn_depth_infinity)
    return SVN_NO_ERROR; /* Nothing to crop */
  if (!(depth >= svn_depth_empty && depth < svn_depth_infinity))
    return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
      _("Can only crop a working copy with a restrictive depth"));

  SVN_ERR(svn_wc__db_read_info(&status, &kind, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, &dir_depth, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL,
                               db, local_abspath,
                               scratch_pool, scratch_pool));

  if (kind != svn_node_dir)
    return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
      _("Can only crop directories"));

  switch (status)
    {
      case svn_wc__db_status_not_present:
      case svn_wc__db_status_server_excluded:
        return svn_error_createf(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
                                 _("The node '%s' was not found."),
                                 svn_dirent_local_style(local_abspath,
                                                        scratch_pool));

      case svn_wc__db_status_deleted:
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                               _("Cannot crop '%s': it is going to be removed "
                                 "from repository. Try commit instead"),
                               svn_dirent_local_style(local_abspath,
                                                      scratch_pool));

      case svn_wc__db_status_added:
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                 _("Cannot crop '%s': it is to be added "
                                   "to the repository. Try commit instead"),
                                 svn_dirent_local_style(local_abspath,
                                                        scratch_pool));
      case svn_wc__db_status_excluded:
        return SVN_NO_ERROR; /* Nothing to do */

      case svn_wc__db_status_normal:
      case svn_wc__db_status_incomplete:
        break;

      default:
        SVN_ERR_MALFUNCTION();
    }

  SVN_ERR(crop_children(db, local_abspath, dir_depth, depth,
                        notify_func, notify_baton,
                        cancel_func, cancel_baton, scratch_pool));

  return svn_error_trace(svn_wc__wq_run(db, local_abspath,
                                        cancel_func, cancel_baton,
                                        scratch_pool));
}
