/*
 * status.c:  return the status of a working copy dirent
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
#include <apr_strings.h>
#include <apr_pools.h>

#include "svn_pools.h"
#include "client.h"

#include "svn_dirent_uri.h"
#include "svn_delta.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_hash.h"

#include "svn_private_config.h"
#include "private/svn_wc_private.h"


/*** Getting update information ***/

/* Baton for tweak_status.  It wraps a bit of extra functionality
   around the received status func/baton, so we can remember if the
   target was deleted in HEAD and tweak incoming status structures
   accordingly. */
struct status_baton
{
  svn_boolean_t deleted_in_repos;          /* target is deleted in repos */
  apr_hash_t *changelist_hash;             /* keys are changelist names */
  svn_wc_status_func4_t real_status_func;  /* real status function */
  void *real_status_baton;                 /* real status baton */
  const char *anchor_abspath;              /* Absolute path of anchor */
  const char *anchor_relpath;              /* Relative path of anchor */
  svn_wc_context_t *wc_ctx;                /* A working copy context. */
};

/* A status callback function which wraps the *real* status
   function/baton.   This sucker takes care of any status tweaks we
   need to make (such as noting that the target of the status is
   missing from HEAD in the repository).

   This implements the 'svn_wc_status_func4_t' function type.  */
static svn_error_t *
tweak_status(void *baton,
             const char *path,
             const svn_wc_status2_t *status,
             apr_pool_t *scratch_pool)
{
  struct status_baton *sb = baton;
  const char *local_abspath;

  SVN_ERR(svn_dirent_get_absolute(&local_abspath, path, scratch_pool));

  /* If we know that the target was deleted in HEAD of the repository,
     we need to note that fact in all the status structures that come
     through here. */
  if (sb->deleted_in_repos)
    {
      svn_wc_status2_t *new_status = svn_wc_dup_status2(status, scratch_pool);
      new_status->repos_text_status = svn_wc_status_deleted;
      status = new_status;
    }

  if (sb->anchor_abspath && svn_dirent_is_absolute(path))
    path = svn_dirent_join(sb->anchor_relpath,
                           svn_dirent_skip_ancestor(sb->anchor_abspath, path),
                           scratch_pool);

  /* If the status item has an entry, but doesn't belong to one of the
     changelists our caller is interested in, we filter our this status
     transmission.  */
  if (! svn_wc__changelist_match(sb->wc_ctx, local_abspath,
                                 sb->changelist_hash, scratch_pool))
    return SVN_NO_ERROR;

  /* Call the real status function/baton. */
  return sb->real_status_func(sb->real_status_baton, path, status,
                              scratch_pool);
}

/* A baton for our reporter that is used to collect locks. */
typedef struct report_baton_t {
  const svn_ra_reporter3_t* wrapped_reporter;
  void *wrapped_report_baton;
  /* The common ancestor URL of all paths included in the report. */
  char *ancestor;
  void *set_locks_baton;
  svn_client_ctx_t *ctx;
  /* Pool to store locks in. */
  apr_pool_t *pool;
} report_baton_t;

/* Implements svn_ra_reporter3_t->set_path. */
static svn_error_t *
reporter_set_path(void *report_baton, const char *path,
                  svn_revnum_t revision, svn_depth_t depth,
                  svn_boolean_t start_empty, const char *lock_token,
                  apr_pool_t *pool)
{
  report_baton_t *rb = report_baton;

  return rb->wrapped_reporter->set_path(rb->wrapped_report_baton, path,
                                        revision, depth, start_empty,
                                        lock_token, pool);
}

/* Implements svn_ra_reporter3_t->delete_path. */
static svn_error_t *
reporter_delete_path(void *report_baton, const char *path, apr_pool_t *pool)
{
  report_baton_t *rb = report_baton;

  return rb->wrapped_reporter->delete_path(rb->wrapped_report_baton, path,
                                           pool);
}

/* Implements svn_ra_reporter3_t->link_path. */
static svn_error_t *
reporter_link_path(void *report_baton, const char *path, const char *url,
                   svn_revnum_t revision, svn_depth_t depth,
                   svn_boolean_t start_empty,
                   const char *lock_token, apr_pool_t *pool)
{
  report_baton_t *rb = report_baton;
  const char *ancestor;
  apr_size_t len;

  ancestor = svn_dirent_get_longest_ancestor(url, rb->ancestor, pool);

  /* If we got a shorter ancestor, truncate our current ancestor.
     Note that svn_dirent_get_longest_ancestor will allocate its return
     value even if it identical to one of its arguments. */
  len = strlen(ancestor);
  if (len < strlen(rb->ancestor))
    rb->ancestor[len] = '\0';

  return rb->wrapped_reporter->link_path(rb->wrapped_report_baton, path, url,
                                         revision, depth, start_empty,
                                         lock_token, pool);
}

/* Implements svn_ra_reporter3_t->finish_report. */
static svn_error_t *
reporter_finish_report(void *report_baton, apr_pool_t *pool)
{
  report_baton_t *rb = report_baton;
  svn_ra_session_t *ras;
  apr_hash_t *locks;
  const char *repos_root;
  apr_pool_t *subpool = svn_pool_create(pool);
  svn_error_t *err = SVN_NO_ERROR;

  /* Open an RA session to our common ancestor and grab the locks under it.
   */
  SVN_ERR(svn_client__open_ra_session_internal(&ras, rb->ancestor, NULL,
                                               NULL, FALSE, TRUE,
                                               rb->ctx, subpool));

  /* The locks need to live throughout the edit.  Note that if the
     server doesn't support lock discovery, we'll just not do locky
     stuff. */
  err = svn_ra_get_locks(ras, &locks, "", rb->pool);
  if (err && ((err->apr_err == SVN_ERR_RA_NOT_IMPLEMENTED)
              || (err->apr_err == SVN_ERR_UNSUPPORTED_FEATURE)))
    {
      svn_error_clear(err);
      err = SVN_NO_ERROR;
      locks = apr_hash_make(rb->pool);
    }
  SVN_ERR(err);

  SVN_ERR(svn_ra_get_repos_root2(ras, &repos_root, rb->pool));

  /* Close the RA session. */
  svn_pool_destroy(subpool);

  SVN_ERR(svn_wc_status_set_repos_locks(rb->set_locks_baton, locks,
                                        repos_root, rb->pool));

  return rb->wrapped_reporter->finish_report(rb->wrapped_report_baton, pool);
}

/* Implements svn_ra_reporter3_t->abort_report. */
static svn_error_t *
reporter_abort_report(void *report_baton, apr_pool_t *pool)
{
  report_baton_t *rb = report_baton;

  return rb->wrapped_reporter->abort_report(rb->wrapped_report_baton, pool);
}

/* A reporter that keeps track of the common URL ancestor of all paths in
   the WC and fetches repository locks for all paths under this ancestor. */
static svn_ra_reporter3_t lock_fetch_reporter = {
  reporter_set_path,
  reporter_delete_path,
  reporter_link_path,
  reporter_finish_report,
  reporter_abort_report
};


/*** Public Interface. ***/


svn_error_t *
svn_client_status5(svn_revnum_t *result_rev,
                   const char *path,
                   const svn_opt_revision_t *revision,
                   svn_wc_status_func4_t status_func,
                   void *status_baton,
                   svn_depth_t depth,
                   svn_boolean_t get_all,
                   svn_boolean_t update,
                   svn_boolean_t no_ignore,
                   svn_boolean_t ignore_externals,
                   const apr_array_header_t *changelists,
                   svn_client_ctx_t *ctx,
                   apr_pool_t *pool)  /* ### aka scratch_pool */
{
  svn_wc_adm_access_t *anchor_access, *target_access;
  const char *anchor, *target;
  const svn_delta_editor_t *editor;
  void *edit_baton, *set_locks_baton;
  const svn_wc_entry_t *entry = NULL;
  struct status_baton sb;
  const char *target_abspath;
  const char *anchor_abspath;
  apr_array_header_t *ignores;
  svn_error_t *err;
  apr_hash_t *changelist_hash = NULL;
  svn_revnum_t edit_revision = SVN_INVALID_REVNUM;
  svn_wc_context_t *wc_ctx = ctx->wc_ctx;
  struct svn_cl__externals_store externals_store = { NULL };

  if (changelists && changelists->nelts)
    SVN_ERR(svn_hash_from_cstring_keys(&changelist_hash, changelists, pool));

  sb.real_status_func = status_func;
  sb.real_status_baton = status_baton;
  sb.deleted_in_repos = FALSE;
  sb.changelist_hash = changelist_hash;
  sb.wc_ctx = ctx->wc_ctx;

/* svn_client_status() is called from other operations that sometimes
         have existing access batons. Run status in a new wc_ctx until we
         get the status running without access batons. We can't pass a
         context and an unrelated access baton to other functions */

  SVN_ERR(svn_wc_context_create(&wc_ctx, NULL, pool, pool));

  /* Try to open the target directory. If the target is a file or an
     unversioned directory, open the parent directory instead */
  err = svn_wc__adm_open_in_context(&anchor_access, wc_ctx, path, FALSE,
                                    SVN_DEPTH_IS_RECURSIVE(depth) ? -1 : 1,
                                    ctx->cancel_func, ctx->cancel_baton,
                                    pool);
  if (err && err->apr_err == SVN_ERR_WC_NOT_DIRECTORY)
    {
      svn_error_clear(err);

      SVN_ERR(svn_wc__adm_open_anchor_in_context(
                                     &anchor_access, &target_access, &target,
                                     wc_ctx, path, FALSE,
                                     SVN_DEPTH_IS_RECURSIVE(depth) ? -1 : 1,
                                     ctx->cancel_func, ctx->cancel_baton,
                                     pool));
    }
  else if (!err)
    {
      target = "";
      target_access = anchor_access;
    }
  else
    return svn_error_return(err);

  anchor = svn_wc_adm_access_path(anchor_access);
  SVN_ERR(svn_dirent_get_absolute(&anchor_abspath, anchor, pool));
  target_abspath = svn_dirent_join(anchor_abspath, target, pool);

  if (svn_dirent_is_absolute(anchor))
    {
      sb.anchor_abspath = NULL;
      sb.anchor_relpath = NULL;
    }
  else
    {
      sb.anchor_abspath = anchor_abspath;
      sb.anchor_relpath = anchor;
    }

  if (!ignore_externals)
    {
      externals_store.pool = pool;
      externals_store.externals_new = apr_hash_make(pool);
    }

  /* Get the status edit, and use our wrapping status function/baton
     as the callback pair. */
  SVN_ERR(svn_wc_get_default_ignores(&ignores, ctx->config, pool));
  SVN_ERR(svn_wc_get_status_editor5(&editor, &edit_baton, &set_locks_baton,
                                    &edit_revision, wc_ctx, anchor_access,
                                    target, depth, get_all, no_ignore,
                                    ignores, tweak_status, &sb,
                                    ctx->cancel_func, ctx->cancel_baton,
                                    ignore_externals ? svn_cl__store_externals
                                                     : NULL,
                                    ignore_externals ? &externals_store
                                                     : NULL,
                                    pool, pool));

  /* If we want to know about out-of-dateness, we crawl the working copy and
     let the RA layer drive the editor for real.  Otherwise, we just close the
     edit.  :-) */
  if (update)
    {
      svn_ra_session_t *ra_session;
      const char *URL;
      svn_node_kind_t kind;
      svn_boolean_t server_supports_depth;

      /* Get full URL from the ANCHOR. */
      if (! entry)
        SVN_ERR(svn_wc__get_entry_versioned(&entry, wc_ctx, anchor_abspath,
                                            svn_node_unknown, FALSE, FALSE,
                                            pool, pool));
      if (! entry->url)
        return svn_error_createf
          (SVN_ERR_ENTRY_MISSING_URL, NULL,
           _("Entry '%s' has no URL"),
           svn_dirent_local_style(anchor, pool));
      URL = apr_pstrdup(pool, entry->url);

      /* Open a repository session to the URL. */
      SVN_ERR(svn_client__open_ra_session_internal(&ra_session, URL, anchor,
                                                   NULL, FALSE, TRUE,
                                                   ctx, pool));

      /* Verify that URL exists in HEAD.  If it doesn't, this can save
         us a whole lot of hassle; if it does, the cost of this
         request should be minimal compared to the size of getting
         back the average amount of "out-of-date" information. */
      SVN_ERR(svn_ra_check_path(ra_session, "", SVN_INVALID_REVNUM,
                                &kind, pool));
      if (kind == svn_node_none)
        {
          /* Our status target does not exist in HEAD of the
             repository.  If we're just adding this thing, that's
             fine.  But if it was previously versioned, then it must
             have been deleted from the repository. */
          if (entry->schedule != svn_wc_schedule_add)
            sb.deleted_in_repos = TRUE;

          /* And now close the edit. */
          SVN_ERR(editor->close_edit(edit_baton, pool));
        }
      else
        {
          svn_revnum_t revnum;
          report_baton_t rb;

          if (revision->kind == svn_opt_revision_head)
            {
              /* Cause the revision number to be omitted from the request,
                 which implies HEAD. */
              revnum = SVN_INVALID_REVNUM;
            }
          else
            {
              /* Get a revision number for our status operation. */
              SVN_ERR(svn_client__get_revision_number(&revnum, NULL,
                                                      wc_ctx,
                                                      target_abspath,
                                                      ra_session, revision,
                                                      pool));
            }

          /* Do the deed.  Let the RA layer drive the status editor. */
          SVN_ERR(svn_ra_do_status2(ra_session, &rb.wrapped_reporter,
                                    &rb.wrapped_report_baton,
                                    target, revnum, depth, editor,
                                    edit_baton, pool));

          /* Init the report baton. */
          rb.ancestor = apr_pstrdup(pool, URL);
          rb.set_locks_baton = set_locks_baton;
          rb.ctx = ctx;
          rb.pool = pool;

          SVN_ERR(svn_ra_has_capability(ra_session, &server_supports_depth,
                                        SVN_RA_CAPABILITY_DEPTH, pool));

          /* Drive the reporter structure, describing the revisions
             within PATH.  When we call reporter->finish_report,
             EDITOR will be driven to describe differences between our
             working copy and HEAD. */
          SVN_ERR(svn_wc_crawl_revisions5(wc_ctx, anchor_abspath, target_abspath,
                                          &lock_fetch_reporter, &rb, FALSE,
                                          depth, TRUE, (! server_supports_depth),
                                          FALSE, NULL, NULL, NULL, NULL, pool));
        }
    }
  else
    {
      SVN_ERR(editor->close_edit(edit_baton, pool));
    }

  if (ctx->notify_func2 && update)
    {
      svn_wc_notify_t *notify
        = svn_wc_create_notify(path, svn_wc_notify_status_completed, pool);
      notify->revision = edit_revision;
      (ctx->notify_func2)(ctx->notify_baton2, notify, pool);
    }

  /* If the caller wants the result revision, give it to them. */
  if (result_rev)
    *result_rev = edit_revision;

  /* Close the access baton here, as svn_client__do_external_status()
     calls back into this function and thus will be re-opening the
     working copy. */
  SVN_ERR(svn_wc_adm_close2(anchor_access, pool));

  /* If there are svn:externals set, we don't want those to show up as
     unversioned or unrecognized, so patch up the hash.  If caller wants
     all the statuses, we will change unversioned status items that
     are interesting to an svn:externals property to
     svn_wc_status_unversioned, otherwise we'll just remove the status
     item altogether.

     We only descend into an external if depth is svn_depth_infinity or
     svn_depth_unknown.  However, there are conceivable behaviors that
     would involve descending under other circumstances; thus, we pass
     depth anyway, so the code will DTRT if we change the conditional
     in the future.
  */
  if (SVN_DEPTH_IS_RECURSIVE(depth) && (! ignore_externals))
    SVN_ERR(svn_client__do_external_status(externals_store.externals_new,
                                           status_func, status_baton,
                                           depth, get_all,
                                           update, no_ignore, ctx, pool));

  return SVN_NO_ERROR;
}
