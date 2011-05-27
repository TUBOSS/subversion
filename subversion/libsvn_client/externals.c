/*
 * externals.c:  handle the svn:externals property
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

#include <apr_uri.h>
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_types.h"
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_props.h"
#include "svn_config.h"
#include "client.h"

#include "svn_private_config.h"
#include "private/svn_wc_private.h"

/* #define USE_EXTERNALS_STORE */

/* Closure for handle_external_item_change. */
struct external_change_baton_t
{
  /* The URL for the repository root. */
  const char *repos_root_url;

  /* Passed through to svn_client_* functions. */
  svn_client_ctx_t *ctx;

  /* Passed to svn_client_exportX() */
  const char *native_eol;

  svn_boolean_t *timestamp_sleep;
  svn_boolean_t is_export;
};

/* Remove the directory at LOCAL_ABSPATH from revision control, and do the
 * same to any revision controlled directories underneath LOCAL_ABSPATH
 * (including directories not referred to by parent svn administrative areas);
 * then if LOCAL_ABSPATH is empty afterwards, remove it, else rename it to a
 * unique name in the same parent directory.
 *
 * Pass CANCEL_FUNC, CANCEL_BATON to svn_wc_remove_from_revision_control.
 *
 * Use SCRATCH_POOL for all temporary allocation.
 */
static svn_error_t *
relegate_dir_external(svn_wc_context_t *wc_ctx,
                      const char *wri_abspath,
                      const char *local_abspath,
                      svn_cancel_func_t cancel_func,
                      void *cancel_baton,
                      apr_pool_t *scratch_pool)
{
  svn_error_t *err = SVN_NO_ERROR;

  err = svn_wc__external_remove(wc_ctx, wri_abspath, local_abspath,
                                cancel_func, cancel_baton, scratch_pool);
  if (err && (err->apr_err == SVN_ERR_WC_LEFT_LOCAL_MOD))
    {
      const char *parent_dir;
      const char *dirname;
      const char *new_path;

      svn_error_clear(err);
      err = SVN_NO_ERROR;

      svn_dirent_split(&parent_dir, &dirname, local_abspath, scratch_pool);

      /* Reserve the new dir name. */
      SVN_ERR(svn_io_open_uniquely_named(NULL, &new_path,
                                         parent_dir, dirname, ".OLD",
                                         svn_io_file_del_none,
                                         scratch_pool, scratch_pool));

      /* Sigh...  We must fall ever so slightly from grace.

         Ideally, there would be no window, however brief, when we
         don't have a reservation on the new name.  Unfortunately,
         at least in the Unix (Linux?) version of apr_file_rename(),
         you can't rename a directory over a file, because it's just
         calling stdio rename(), which says:

            ENOTDIR
              A  component used as a directory in oldpath or newpath
              path is not, in fact, a directory.  Or, oldpath  is
              a directory, and newpath exists but is not a directory

         So instead, we get the name, then remove the file (ugh), then
         rename the directory, hoping that nobody has gotten that name
         in the meantime -- which would never happen in real life, so
         no big deal.
      */
      /* Do our best, but no biggy if it fails. The rename will fail. */
      svn_error_clear(svn_io_remove_file2(new_path, TRUE, scratch_pool));

      /* Rename. */
      SVN_ERR(svn_io_file_rename(local_abspath, new_path, scratch_pool));
    }

  return svn_error_return(err);
}

/* Try to update a directory external at PATH to URL at REVISION.
   Use POOL for temporary allocations, and use the client context CTX. */
static svn_error_t *
switch_dir_external(const char *local_abspath,
                    const char *url,
                    const svn_opt_revision_t *revision,
                    const svn_opt_revision_t *peg_revision,
                    const char *defining_abspath,
                    svn_boolean_t *timestamp_sleep,
                    svn_client_ctx_t *ctx,
                    apr_pool_t *pool)
{
  svn_node_kind_t kind;
  svn_error_t *err;
  apr_pool_t *subpool = svn_pool_create(pool);

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  /* If path is a directory, try to update/switch to the correct URL
     and revision. */
  SVN_ERR(svn_io_check_path(local_abspath, &kind, pool));
  if (kind == svn_node_dir)
    {
      const char *node_url;

      /* Doubles as an "is versioned" check. */
      err = svn_wc__node_get_url(&node_url, ctx->wc_ctx, local_abspath,
                                 pool, subpool);
      if (err && err->apr_err == SVN_ERR_WC_PATH_NOT_FOUND)
        {
          svn_error_clear(err);
          err = SVN_NO_ERROR;
          goto relegate;
        }
      else if (err)
        return svn_error_return(err);

      if (node_url)
        {
          const char *repos_root_url;

          /* If we have what appears to be a version controlled
             subdir, and its top-level URL matches that of our
             externals definition, perform an update. */
          if (strcmp(node_url, url) == 0)
            {
              SVN_ERR(svn_client__update_internal(NULL, local_abspath,
                                                  revision, svn_depth_unknown,
                                                  FALSE, FALSE, FALSE, TRUE,
                                                  FALSE, TRUE,
                                                  timestamp_sleep,
                                                  ctx, subpool));
              svn_pool_destroy(subpool);
              return SVN_NO_ERROR;
            }

          SVN_ERR(svn_wc__node_get_repos_info(&repos_root_url, NULL,
                                              ctx->wc_ctx, local_abspath,
                                              pool, subpool));
          if (repos_root_url)
            {
              /* URLs don't match.  Try to relocate (if necessary) and then
                 switch. */
              if (! svn_uri_is_ancestor(repos_root_url, url))
                {
                  const char *repos_root;
                  svn_ra_session_t *ra_session;

                  /* Get the repos root of the new URL. */
                  SVN_ERR(svn_client__open_ra_session_internal
                          (&ra_session, NULL, url, NULL, NULL,
                           FALSE, TRUE, ctx, subpool));
                  SVN_ERR(svn_ra_get_repos_root2(ra_session, &repos_root,
                                                 subpool));

                  err = svn_client_relocate2(local_abspath, repos_root_url,
                                             repos_root,
                                             FALSE, ctx, subpool);
                  /* If the relocation failed because the new URL points
                     to another repository, then we need to relegate and
                     check out a new WC. */
                  if (err
                      && (err->apr_err == SVN_ERR_WC_INVALID_RELOCATION
                          || (err->apr_err
                              == SVN_ERR_CLIENT_INVALID_RELOCATION)))
                    {
                      svn_error_clear(err);
                      goto relegate;
                    }
                  else if (err)
                    return svn_error_return(err);
                }

              SVN_ERR(svn_client__switch_internal(NULL, local_abspath, url,
                                                  peg_revision, revision,
                                                  svn_depth_infinity,
                                                  TRUE, FALSE, FALSE,
                                                  TRUE, TRUE,
                                                  timestamp_sleep,
                                                  ctx, subpool));

              {
                const char *repos_root_url;
                const char *repos_uuid;
                const char *repos_relpath;

                SVN_ERR(svn_wc__node_get_repos_info(&repos_root_url,
                                                    &repos_uuid,
                                                    ctx->wc_ctx, local_abspath,
                                                    subpool, subpool));

                SVN_ERR(svn_wc__node_get_repos_relpath(&repos_relpath,
                                                       ctx->wc_ctx,
                                                       local_abspath,
                                                       subpool, subpool));

                SVN_ERR(svn_wc__external_register(ctx->wc_ctx,
                                                  defining_abspath,
                                                  local_abspath, svn_node_dir,
                                                  repos_root_url, repos_uuid,
                                                  repos_relpath,
                                                  SVN_INVALID_REVNUM,
                                                  SVN_INVALID_REVNUM,
                                                  subpool));
              }

              svn_pool_destroy(subpool);
              return SVN_NO_ERROR;
            }
        }
    }

 relegate:

  /* Fall back on removing the WC and checking out a new one. */

  /* Ensure that we don't have any RA sessions or WC locks from failed
     operations above. */
  svn_pool_destroy(subpool);

  if (kind == svn_node_dir)
    {
      /* Buh-bye, old and busted ... */
      SVN_ERR(svn_wc__acquire_write_lock(NULL, ctx->wc_ctx, local_abspath,
                                         FALSE, pool, pool));

      SVN_ERR(relegate_dir_external(ctx->wc_ctx, defining_abspath,
                                    local_abspath,
                                    ctx->cancel_func, ctx->cancel_baton,
                                    pool));
    }
  else
    {
      /* The target dir might have multiple components.  Guarantee
         the path leading down to the last component. */
      const char *parent = svn_dirent_dirname(local_abspath, pool);
      SVN_ERR(svn_io_make_dir_recursively(parent, pool));
    }

  /* ... Hello, new hotness. */
  SVN_ERR(svn_client__checkout_internal(NULL, url, local_abspath, peg_revision,
                                        revision, NULL, svn_depth_infinity,
                                        FALSE, FALSE, TRUE, timestamp_sleep,
                                        ctx, pool));

  {
    const char *repos_root_url;
    const char *repos_uuid;
    const char *repos_relpath;

    SVN_ERR(svn_wc__node_get_repos_info(&repos_root_url,
                                        &repos_uuid,
                                        ctx->wc_ctx, local_abspath,
                                        pool, pool));

    SVN_ERR(svn_wc__node_get_repos_relpath(&repos_relpath,
                                           ctx->wc_ctx,
                                           local_abspath,
                                           pool, pool));

    SVN_ERR(svn_wc__external_register(ctx->wc_ctx,
                                      defining_abspath,
                                      local_abspath, svn_node_dir,
                                      repos_root_url, repos_uuid,
                                      repos_relpath,
                                      SVN_INVALID_REVNUM,
                                      SVN_INVALID_REVNUM,
                                      pool));
  }

  return SVN_NO_ERROR;
}

/* Try to update a file external at LOCAL_ABSPATH to URL at REVISION using a
   access baton that has a write lock.  Use SCRATCH_POOL for temporary
   allocations, and use the client context CTX. */
static svn_error_t *
switch_file_external(const char *local_abspath,
                     const char *url,
                     const svn_opt_revision_t *peg_revision,
                     const svn_opt_revision_t *revision,
                     const char *def_dir_abspath,
                     svn_ra_session_t *ra_session,
                     const char *ra_session_url,
                     svn_revnum_t ra_revnum,
                     const char *repos_root_url,
                     svn_boolean_t *timestamp_sleep,
                     svn_client_ctx_t *ctx,
                     apr_pool_t *scratch_pool)
{
  apr_pool_t *subpool = svn_pool_create(scratch_pool);
  const char *dir_abspath;
  const char *target;
  svn_config_t *cfg = ctx->config ? apr_hash_get(ctx->config,
                                                 SVN_CONFIG_CATEGORY_CONFIG,
                                                 APR_HASH_KEY_STRING) : NULL;
  svn_boolean_t use_commit_times;
  const char *diff3_cmd;
  const char *preserved_exts_str;
  const apr_array_header_t *preserved_exts;
  svn_boolean_t locked_here;
  svn_error_t *err = NULL;
  svn_node_kind_t kind, external_kind;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  /* See if the user wants last-commit timestamps instead of current ones. */
  SVN_ERR(svn_config_get_bool(cfg, &use_commit_times,
                              SVN_CONFIG_SECTION_MISCELLANY,
                              SVN_CONFIG_OPTION_USE_COMMIT_TIMES, FALSE));

  /* Get the external diff3, if any. */
  svn_config_get(cfg, &diff3_cmd, SVN_CONFIG_SECTION_HELPERS,
                 SVN_CONFIG_OPTION_DIFF3_CMD, NULL);

  if (diff3_cmd != NULL)
    SVN_ERR(svn_path_cstring_to_utf8(&diff3_cmd, diff3_cmd, subpool));

  /* See which files the user wants to preserve the extension of when
     conflict files are made. */
  svn_config_get(cfg, &preserved_exts_str, SVN_CONFIG_SECTION_MISCELLANY,
                 SVN_CONFIG_OPTION_PRESERVED_CF_EXTS, "");
  preserved_exts = *preserved_exts_str
    ? svn_cstring_split(preserved_exts_str, "\n\r\t\v ", FALSE, subpool)
    : NULL;

  svn_dirent_split(&dir_abspath, &target, local_abspath, subpool);

  /* Try to get a access baton for the anchor using the input access
     baton.  If this fails and returns SVN_ERR_WC_NOT_LOCKED, then try
     to get a new access baton to support inserting a file external
     into a directory external. */
  SVN_ERR(svn_wc_locked2(&locked_here, NULL, ctx->wc_ctx, dir_abspath,
                         subpool));
  if (!locked_here)
    {
      const char *dest_wc_repos_root_url;

      /* Check that the repository root URL for the newly opened
         wc is the same as the file external. */
      SVN_ERR(svn_client__get_repos_root(&dest_wc_repos_root_url,
                                         dir_abspath,
                                         ctx, subpool, subpool));

      if (0 != strcmp(repos_root_url, dest_wc_repos_root_url))
        return svn_error_createf
          (SVN_ERR_RA_REPOS_ROOT_URL_MISMATCH, NULL,
           _("Cannot insert a file external from '%s' into a working "
             "copy from a different repository rooted at '%s'"),
           url, dest_wc_repos_root_url);

      SVN_ERR(svn_wc__acquire_write_lock(NULL, ctx->wc_ctx, dir_abspath,
                                         FALSE, subpool, subpool));
    }

  err = svn_wc_read_kind(&kind, ctx->wc_ctx, local_abspath, FALSE, subpool);
  if (err)
    goto cleanup;

  err = svn_wc__read_external_info(&external_kind, NULL, NULL, NULL, NULL,
                                   ctx->wc_ctx, local_abspath, local_abspath,
                                   TRUE, scratch_pool, scratch_pool);

  if (err)
    goto cleanup;

  /* If there is a versioned item with this name, ensure it's a file
     external before working with it.  If there is no entry in the
     working copy, then create an empty file and add it to the working
     copy. */
  if (kind != svn_node_none && kind != svn_node_unknown)
    {
      if (external_kind != svn_node_file)
        {
          if (!locked_here)
            SVN_ERR(svn_wc__release_write_lock(ctx->wc_ctx, dir_abspath,
                                               subpool));

          return svn_error_createf(
              SVN_ERR_CLIENT_FILE_EXTERNAL_OVERWRITE_VERSIONED, 0,
             _("The file external from '%s' cannot overwrite the existing "
               "versioned item at '%s'"),
             url, svn_dirent_local_style(local_abspath, subpool));
        }
    }
  else
    {
      svn_node_kind_t disk_kind;

      err = svn_io_check_path(local_abspath, &disk_kind, subpool);

      if (err)
        goto cleanup;

      if (kind == svn_node_file || kind == svn_node_dir)
        {
          err = svn_error_createf(SVN_ERR_WC_PATH_FOUND, NULL,
                                  _("The file external '%s' can not be "
                                    "created because the node exists."),
                                  svn_dirent_local_style(local_abspath,
                                                         subpool));
          goto cleanup;
        }
    }

  {
    const svn_ra_reporter3_t *reporter;
    void *report_baton;
    const svn_delta_editor_t *switch_editor;
    void *switch_baton;
    const char *switch_rev_url;
    const char *repos_uuid;
    svn_revnum_t revnum;
    /* ### TODO: Provide the real definition path (now available in
       ### def_dir_abspath) after switching to the new externals store. 
       ### We can't enable this now, because that would move the external
       ### information into the wrong working copy */
    const char *definition_abspath = svn_dirent_dirname(local_abspath,subpool);

    /* Open an RA session to 'source' URL */
    SVN_ERR(svn_client__ra_session_from_path(&ra_session, &revnum,
                                             &switch_rev_url,
                                             url, dir_abspath,
                                             peg_revision, revision,
                                             ctx, subpool));

    SVN_ERR(svn_ra_reparent(ra_session, url, subpool));
    SVN_ERR(svn_ra_get_uuid2(ra_session, &repos_uuid, subpool));

    SVN_ERR(svn_wc__get_file_external_editor(&switch_editor, &switch_baton,
                                             &revnum, ctx->wc_ctx,
                                             local_abspath,
                                             definition_abspath /* wri */, 
                                             switch_rev_url,
                                             repos_root_url,
                                             repos_uuid,
                                             use_commit_times,
                                             diff3_cmd, preserved_exts,
                                             definition_abspath /* def */,
                                             url, peg_revision, revision,
                                             ctx->conflict_func2,
                                             ctx->conflict_baton2,
                                             ctx->cancel_func,
                                             ctx->cancel_baton,
                                             ctx->notify_func2,
                                             ctx->notify_baton2,
                                             subpool, subpool));

    /* Tell RA to do an update of URL+TARGET to REVISION; if we pass an
     invalid revnum, that means RA will use the latest revision. */
  SVN_ERR(svn_ra_do_switch2(ra_session, &reporter, &report_baton, revnum,
                            target, svn_depth_unknown, url,
                            switch_editor, switch_baton, subpool));

  SVN_ERR(svn_wc__crawl_file_external(ctx->wc_ctx, local_abspath,
                                      definition_abspath /* wri_abspath */,
                                      reporter, report_baton,
                                      TRUE,  use_commit_times,
                                      ctx->cancel_func, ctx->cancel_baton,
                                      ctx->notify_func2, ctx->notify_baton2,
                                      subpool));

    if (ctx->notify_func2)
      {
        svn_wc_notify_t *notify
          = svn_wc_create_notify(local_abspath, svn_wc_notify_update_completed,
                                 subpool);
        notify->kind = svn_node_none;
        notify->content_state = notify->prop_state
          = svn_wc_notify_state_inapplicable;
        notify->lock_state = svn_wc_notify_lock_state_inapplicable;
        notify->revision = revnum;
        (*ctx->notify_func2)(ctx->notify_baton2, notify, subpool);
      }
  }

cleanup:
  if (!locked_here)
    err = svn_error_compose_create(err,
             svn_wc__release_write_lock(ctx->wc_ctx, dir_abspath, subpool));

  svn_pool_destroy(subpool);
  return svn_error_return(err);
}

/* Return the scheme of @a uri in @a scheme allocated from @a pool.
   If @a uri does not appear to be a valid URI, then @a scheme will
   not be updated.  */
static svn_error_t *
uri_scheme(const char **scheme, const char *uri, apr_pool_t *pool)
{
  apr_size_t i;

  for (i = 0; uri[i] && uri[i] != ':'; ++i)
    if (uri[i] == '/')
      goto error;

  if (i > 0 && uri[i] == ':' && uri[i+1] == '/' && uri[i+2] == '/')
    {
      *scheme = apr_pstrmemdup(pool, uri, i);
      return SVN_NO_ERROR;
    }

error:
  return svn_error_createf(SVN_ERR_BAD_URL, 0,
                           _("URL '%s' does not begin with a scheme"),
                           uri);
}

/* If the URL for @a item is relative, then using the repository root
   URL @a repos_root_url and the parent directory URL @parent_dir_url,
   resolve it into an absolute URL and save it in @a item.

   Regardless if the URL is absolute or not, if there are no errors,
   the URL in @a item will be canonicalized.

   The following relative URL formats are supported:

     ../    relative to the parent directory of the external
     ^/     relative to the repository root
     //     relative to the scheme
     /      relative to the server's hostname

   The ../ and ^/ relative URLs may use .. to remove path elements up
   to the server root.

   The external URL should not be canonicalized before calling this function,
   as otherwise the scheme relative URL '//host/some/path' would have been
   canonicalized to '/host/some/path' and we would not be able to match on
   the leading '//'. */
static svn_error_t *
resolve_relative_external_url(const char **resolved_url,
                              const svn_wc_external_item2_t *item,
                              const char *repos_root_url,
                              const char *parent_dir_url,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool)
{
  const char *url = item->url;
  apr_uri_t parent_dir_uri;
  apr_status_t status;

  *resolved_url = item->url;

  /* If the URL is already absolute, there is nothing to do. */
  if (svn_path_is_url(url))
    {
      /* "http://server/path" */
      *resolved_url = svn_uri_canonicalize(url, result_pool);
      return SVN_NO_ERROR;
    }

  if (url[0] == '/')
    {
      /* "/path", "//path", and "///path" */
      int num_leading_slashes = 1;
      if (url[1] == '/')
        {
          num_leading_slashes++;
          if (url[2] == '/')
            num_leading_slashes++;
        }

      /* "//schema-relative" and in some cases "///schema-relative".
         This last format is supported on file:// schema relative. */
      url = apr_pstrcat(scratch_pool,
                        apr_pstrndup(scratch_pool, url, num_leading_slashes),
                        svn_relpath_canonicalize(url + num_leading_slashes,
                                                 scratch_pool),
                        (char*)NULL);
    }
  else
    {
      /* "^/path" and "../path" */
      url = svn_relpath_canonicalize(url, scratch_pool);
    }

  /* Parse the parent directory URL into its parts. */
  status = apr_uri_parse(scratch_pool, parent_dir_url, &parent_dir_uri);
  if (status)
    return svn_error_createf(SVN_ERR_BAD_URL, 0,
                             _("Illegal parent directory URL '%s'"),
                             parent_dir_url);

  /* If the parent directory URL is at the server root, then the URL
     may have no / after the hostname so apr_uri_parse() will leave
     the URL's path as NULL. */
  if (! parent_dir_uri.path)
    parent_dir_uri.path = apr_pstrmemdup(scratch_pool, "/", 1);
  parent_dir_uri.query = NULL;
  parent_dir_uri.fragment = NULL;

  /* Handle URLs relative to the current directory or to the
     repository root.  The backpaths may only remove path elements,
     not the hostname.  This allows an external to refer to another
     repository in the same server relative to the location of this
     repository, say using SVNParentPath. */
  if ((0 == strncmp("../", url, 3)) ||
      (0 == strncmp("^/", url, 2)))
    {
      apr_array_header_t *base_components;
      apr_array_header_t *relative_components;
      int i;

      /* Decompose either the parent directory's URL path or the
         repository root's URL path into components.  */
      if (0 == strncmp("../", url, 3))
        {
          base_components = svn_path_decompose(parent_dir_uri.path,
                                               scratch_pool);
          relative_components = svn_path_decompose(url, scratch_pool);
        }
      else
        {
          apr_uri_t repos_root_uri;

          status = apr_uri_parse(scratch_pool, repos_root_url,
                                 &repos_root_uri);
          if (status)
            return svn_error_createf(SVN_ERR_BAD_URL, 0,
                                     _("Illegal repository root URL '%s'"),
                                     repos_root_url);

          /* If the repository root URL is at the server root, then
             the URL may have no / after the hostname so
             apr_uri_parse() will leave the URL's path as NULL. */
          if (! repos_root_uri.path)
            repos_root_uri.path = apr_pstrmemdup(scratch_pool, "/", 1);

          base_components = svn_path_decompose(repos_root_uri.path,
                                               scratch_pool);
          relative_components = svn_path_decompose(url + 2, scratch_pool);
        }

      for (i = 0; i < relative_components->nelts; ++i)
        {
          const char *component = APR_ARRAY_IDX(relative_components,
                                                i,
                                                const char *);
          if (0 == strcmp("..", component))
            {
              /* Constructing the final absolute URL together with
                 apr_uri_unparse() requires that the path be absolute,
                 so only pop a component if the component being popped
                 is not the component for the root directory. */
              if (base_components->nelts > 1)
                apr_array_pop(base_components);
            }
          else
            APR_ARRAY_PUSH(base_components, const char *) = component;
        }

      parent_dir_uri.path = (char *)svn_path_compose(base_components,
                                                     scratch_pool);
      *resolved_url = svn_uri_canonicalize(apr_uri_unparse(scratch_pool,
                                                           &parent_dir_uri, 0),
                                       result_pool);
      return SVN_NO_ERROR;
    }

  /* The remaining URLs are relative to the either the scheme or
     server root and can only refer to locations inside that scope, so
     backpaths are not allowed. */
  if (svn_path_is_backpath_present(url + 2))
    return svn_error_createf(SVN_ERR_BAD_URL, 0,
                             _("The external relative URL '%s' cannot have "
                               "backpaths, i.e. '..'"),
                             item->url);

  /* Relative to the scheme: Build a new URL from the parts we know.  */
  if (0 == strncmp("//", url, 2))
    {
      const char *scheme;

      SVN_ERR(uri_scheme(&scheme, repos_root_url, scratch_pool));
      *resolved_url = svn_uri_canonicalize(apr_pstrcat(scratch_pool, scheme,
                                                       ":", url, (char *)NULL),
                                           result_pool);
      return SVN_NO_ERROR;
    }

  /* Relative to the server root: Just replace the path portion of the
     parent's URL.  */
  if (url[0] == '/')
    {
      parent_dir_uri.path = (char *)url;
      *resolved_url = svn_uri_canonicalize(apr_uri_unparse(scratch_pool,
                                                           &parent_dir_uri, 0),
                                           result_pool);
      return SVN_NO_ERROR;
    }

  return svn_error_createf(SVN_ERR_BAD_URL, 0,
                           _("Unrecognized format for the relative external "
                             "URL '%s'"),
                           item->url);
}

static svn_error_t *
handle_external_item_removal(const struct external_change_baton_t *eb,
                             const char *wri_abspath,
                             const char *local_abspath,
                             const char *old_url,
                             apr_pool_t *scratch_pool)
{
  /* This branch is only used when an external is deleted from the
     repository and the working copy is updated or committed. */

  svn_error_t *err;
  svn_boolean_t lock_existed;
  svn_node_kind_t kind;
  const char *lock_root_abspath = NULL;

  /* local_abspath should be a wcroot or a file external */
  SVN_ERR(svn_wc_read_kind(&kind, eb->ctx->wc_ctx, local_abspath, FALSE,
                           scratch_pool));

  if (kind == svn_node_none)
    return SVN_NO_ERROR; /* It's neither... Nothing to remove */

  SVN_ERR(svn_wc_locked2(&lock_existed, NULL, eb->ctx->wc_ctx,
                         local_abspath, scratch_pool));

  if (! lock_existed)
    {
      SVN_ERR(svn_wc__acquire_write_lock(&lock_root_abspath,
                                         eb->ctx->wc_ctx, local_abspath,
                                         FALSE,
                                         scratch_pool,
                                         scratch_pool));
    }

  /* We don't use relegate_dir_external() here, because we know that
     nothing else in this externals description (at least) is
     going to need this directory, and therefore it's better to
     leave stuff where the user expects it. */
  err = svn_wc__external_remove(eb->ctx->wc_ctx, wri_abspath, local_abspath,
                                eb->ctx->cancel_func, eb->ctx->cancel_baton,
                                scratch_pool);

  if (eb->ctx->notify_func2)
    {
      svn_wc_notify_t *notify =
          svn_wc_create_notify(local_abspath,
                               svn_wc_notify_update_external_removed,
                               scratch_pool);

      notify->kind = svn_node_dir;
      notify->err = err;

      (eb->ctx->notify_func2)(eb->ctx->notify_baton2,
                              notify, scratch_pool);
    }

  if (err && err->apr_err == SVN_ERR_WC_LEFT_LOCAL_MOD)
    {
      svn_error_clear(err);
      err = NULL;
    }


  /* Unlock if we acquired the lock */
  if (lock_root_abspath != NULL)
    {
      svn_error_t *err2 = svn_wc__release_write_lock(eb->ctx->wc_ctx,
                                                     lock_root_abspath,
                                                     scratch_pool);

      if (err2 && err2->apr_err == SVN_ERR_WC_NOT_LOCKED)
        {
          /* We removed the lock by removing the node, how nice! */
          svn_error_clear(err2);
        }
      else
        err = svn_error_compose_create(err, err2);
    }

  return svn_error_return(err);
}

static svn_error_t *
handle_external_item_change(const struct external_change_baton_t *eb,
                            const char *parent_dir_abspath,
                            const char *parent_dir_url,
                            const char *local_abspath,
                            const char *old_url,
                            const svn_wc_external_item2_t *new_item,
                            apr_pool_t *scratch_pool)
{
  svn_ra_session_t *ra_session;
  svn_client__ra_session_from_path_results ra_cache = { 0 };
  const char *new_url;

  ra_cache.kind = svn_node_unknown;

  SVN_ERR_ASSERT(eb->repos_root_url && parent_dir_url);
  SVN_ERR_ASSERT(new_item != NULL);

  /* Don't bother to check status, since we'll get that for free by
     attempting to retrieve the hash values anyway.  */

  /* When creating the absolute URL, use the pool and not the
     iterpool, since the hash table values outlive the iterpool and
     any pointers they have should also outlive the iterpool.  */

   SVN_ERR(resolve_relative_external_url(&new_url,
                                         new_item, eb->repos_root_url,
                                         parent_dir_url,
                                         scratch_pool, scratch_pool));

  /* There's one potential ugliness.  If a target subdir changed, but
     its URL did not, then ideally we'd just rename the subdir, rather
     than remove the old subdir only to do a new checkout into the new
     subdir.

     We could solve this by "sneaking around the back" and looking in
     eb->new_desc, eb->old_desc to check if anything else in this
     parent_dir has the same URL.  Of course, if an external gets
     moved into some other directory, then we'd lose anyway.  The only
     way to fully handle this would be to harvest a global list based
     on urls/revs, and consult the list every time we're about to
     delete an external subdir: whenever a deletion is really part of
     a rename, then we'd do the rename on the spot.

     IMHO, renames aren't going to be frequent enough to make the
     extra bookkeeping worthwhile.
  */

  /* If the external is being checked out, exported or updated,
     determine if the external is a file or directory. */
  if (new_item)
    {
      svn_node_kind_t kind;

      /* Get the RA connection. */
      SVN_ERR(svn_client__ra_session_from_path(&ra_session,
                                               &ra_cache.ra_revnum,
                                               &ra_cache.ra_session_url,
                                               new_url, NULL,
                                               &(new_item->peg_revision),
                                               &(new_item->revision), eb->ctx,
                                               scratch_pool));

      SVN_ERR(svn_ra_get_uuid2(ra_session, &ra_cache.repos_uuid,
                               scratch_pool));
      SVN_ERR(svn_ra_get_repos_root2(ra_session, &ra_cache.repos_root_url,
                                     scratch_pool));
      SVN_ERR(svn_ra_check_path(ra_session, "", ra_cache.ra_revnum, &kind,
                                scratch_pool));

      if (svn_node_none == kind)
        return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
                                 _("URL '%s' at revision %ld doesn't exist"),
                                 ra_cache.ra_session_url,
                                 ra_cache.ra_revnum);

      if (svn_node_dir != kind && svn_node_file != kind)
        return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
                                 _("URL '%s' at revision %ld is not a file "
                                   "or a directory"),
                                 ra_cache.ra_session_url,
                                 ra_cache.ra_revnum);

      ra_cache.kind = kind;
    }

  /* Not protecting against recursive externals.  Detecting them in
     the global case is hard, and it should be pretty obvious to a
     user when it happens.  Worst case: your disk fills up :-). */

  if (! old_url)
    {
      /* This branch is only used during a checkout or an export. */

      /* First notify that we're about to handle an external. */
      if (eb->ctx->notify_func2)
        (*eb->ctx->notify_func2)
          (eb->ctx->notify_baton2,
           svn_wc_create_notify(local_abspath, svn_wc_notify_update_external,
                                scratch_pool), scratch_pool);

      switch (ra_cache.kind)
        {
        case svn_node_dir:
          /* The target dir might have multiple components.  Guarantee
             the path leading down to the last component. */
          SVN_ERR(svn_io_make_dir_recursively(svn_dirent_dirname(local_abspath,
                                                                 scratch_pool),
                                              scratch_pool));

          /* If we were handling renames the fancy way, then before
             checking out a new subdir here, we would somehow learn if
             it's really just a rename of an old one.  That would work in
             tandem with the next case -- this case would do nothing,
             knowing that the next case either already has, or soon will,
             rename the external subdirectory. */

          if (eb->is_export)
            /* ### It should be okay to "force" this export.  Externals
               only get created in subdirectories of versioned
               directories, so an external directory couldn't already
               exist before the parent export process unless a versioned
               directory above it did, which means the user would have
               already had to force these creations to occur. */
            SVN_ERR(svn_client_export4(NULL, new_url, local_abspath,
                                       &(new_item->peg_revision),
                                       &(new_item->revision),
                                       TRUE, FALSE, svn_depth_infinity,
                                       eb->native_eol,
                                       eb->ctx, scratch_pool));
          else
            SVN_ERR(switch_dir_external(
                     local_abspath, new_url,
                     &(new_item->peg_revision), &(new_item->revision),
                     parent_dir_abspath, eb->timestamp_sleep, eb->ctx,
                     scratch_pool));
          break;
        case svn_node_file:
          if (eb->is_export)
            /* Do not overwrite an existing file with this file
               external. */
            SVN_ERR(svn_client_export4(NULL, new_url, local_abspath,
                                       &(new_item->peg_revision),
                                       &(new_item->revision),
                                       FALSE, TRUE, svn_depth_infinity,
                                       eb->native_eol,
                                       eb->ctx, scratch_pool));
          else
            SVN_ERR(switch_file_external(local_abspath,
                                         new_url,
                                         &new_item->peg_revision,
                                         &new_item->revision,
                                         parent_dir_abspath,
                                         ra_session,
                                         ra_cache.ra_session_url,
                                         ra_cache.ra_revnum,
                                         ra_cache.repos_root_url,
                                         eb->timestamp_sleep, eb->ctx,
                                         scratch_pool));
          break;
        default:
          SVN_ERR_MALFUNCTION();
          break;
        }
    }
  else
    {
      /* This branch handles a definition change or simple update. */

      /* First notify that we're about to handle an external. */
      if (eb->ctx->notify_func2)
        {
          svn_wc_notify_t *nt;

          nt = svn_wc_create_notify(local_abspath,
                                    svn_wc_notify_update_external,
                                    scratch_pool);

          eb->ctx->notify_func2(eb->ctx->notify_baton2, nt, scratch_pool);
        }

      /* Either the URL changed, or the exact same item is present in
         both hashes, and caller wants to update such unchanged items.
         In the latter case, the call below will try to make sure that
         the external really is a WC pointing to the correct
         URL/revision. */
      switch (ra_cache.kind)
        {
        case svn_node_dir:
          SVN_ERR(switch_dir_external(local_abspath, new_url,
                                      &(new_item->revision),
                                      &(new_item->peg_revision),
                                      parent_dir_abspath,
                                      eb->timestamp_sleep, eb->ctx,
                                      scratch_pool));
          break;
        case svn_node_file:
          SVN_ERR(switch_file_external(local_abspath,
                                       new_url,
                                       &new_item->peg_revision,
                                       &new_item->revision,
                                       parent_dir_abspath,
                                       ra_session,
                                       ra_cache.ra_session_url,
                                       ra_cache.ra_revnum,
                                       ra_cache.repos_root_url,
                                       eb->timestamp_sleep, eb->ctx,
                                       scratch_pool));
          break;
        default:
          SVN_ERR_MALFUNCTION();
          break;
        }
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
wrap_external_error(const struct external_change_baton_t *eb,
                    const char *target_abspath,
                    svn_error_t *err,
                    apr_pool_t *scratch_pool)
{
  /* In maintainer mode we prefer to fail on external processing errors, instead
     of just notifying them as a warning. (The warnings would be skipped
     by the test suite) */
#ifndef SVN_DEBUG
  if (err && err->apr_err != SVN_ERR_CANCELLED)
    {
      if (eb->ctx->notify_func2)
        {
          svn_wc_notify_t *notifier = svn_wc_create_notify(
                                            target_abspath,
                                            svn_wc_notify_failed_external,
                                            scratch_pool);
          notifier->err = err;
          eb->ctx->notify_func2(eb->ctx->notify_baton2, notifier,
                                scratch_pool);
        }
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }
#endif

  return err;
}

/* This implements the 'svn_hash_diff_func_t' interface.
   BATON is of type 'struct handle_externals_desc_change_baton *'.
   KEY is a 'const char *'.
*/
static svn_error_t *
handle_externals_change(const struct external_change_baton_t *eb,
                        const char *local_abspath,
                        const char *new_desc_text,
                        apr_hash_t *old_url_hash,
                        svn_depth_t ambient_depth,
                        svn_depth_t requested_depth,
                        apr_pool_t *scratch_pool)
{
  apr_array_header_t *new_desc;
  int i;
  apr_pool_t *iterpool;
  const char *url;

  iterpool = svn_pool_create(scratch_pool);

  SVN_ERR_ASSERT(svn_dirent_is_absolute(local_abspath));

  /* Bag out if the depth here is too shallow for externals action. */
  if ((requested_depth < svn_depth_infinity
       && requested_depth != svn_depth_unknown)
      || (ambient_depth < svn_depth_infinity
          && requested_depth < svn_depth_infinity))
    return SVN_NO_ERROR;

  if (new_desc_text)
    SVN_ERR(svn_wc_parse_externals_description3(&new_desc, local_abspath,
                                                new_desc_text,
                                                FALSE, scratch_pool));
  else
    new_desc = NULL;

  SVN_ERR(svn_wc__node_get_url(&url, eb->ctx->wc_ctx, local_abspath,
                               scratch_pool, iterpool));

  SVN_ERR_ASSERT(url);

  for (i = 0; new_desc && (i < new_desc->nelts); i++)
    {
      const char *old_url;
      svn_wc_external_item2_t *new_item;
      const char *target_abspath;

      new_item = APR_ARRAY_IDX(new_desc, i, svn_wc_external_item2_t *);

      svn_pool_clear(iterpool);

      target_abspath = svn_dirent_join(local_abspath, new_item->target_dir,
                                       iterpool);

      old_url = apr_hash_get(old_url_hash, target_abspath,
                              APR_HASH_KEY_STRING);

      SVN_ERR(wrap_external_error(
                      eb, target_abspath,
                      handle_external_item_change(eb, local_abspath, url,
                                                  target_abspath,
                                                  old_url, new_item,
                                                  iterpool),
                      iterpool));

      /* And remove already processed items from the to-remove hash */
      if (old_url)
        apr_hash_set(old_url_hash, target_abspath, APR_HASH_KEY_STRING, NULL);
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_client__handle_externals(apr_hash_t *externals_old,
                             apr_hash_t *externals_new,
                             apr_hash_t *ambient_depths,
                             const char *repos_root_url,
                             const char *target_abspath,
                             svn_depth_t requested_depth,
                             svn_boolean_t *timestamp_sleep,
                             svn_client_ctx_t *ctx,
                             apr_pool_t *scratch_pool)
{
  apr_hash_t *combined;
  apr_hash_index_t *hi;
  apr_pool_t *iterpool;
  apr_hash_t *old_external_urls;
  struct external_change_baton_t eb;

  SVN_ERR_ASSERT(repos_root_url);

  eb.repos_root_url = repos_root_url;
  eb.ctx = ctx;
  eb.native_eol = NULL;
  eb.timestamp_sleep = timestamp_sleep;
  eb.is_export = FALSE;

  if (! externals_old)
    combined = externals_new;
  else if (! externals_new)
    combined = externals_old;
  else
    combined = apr_hash_overlay(scratch_pool, externals_old, externals_new);

  iterpool = svn_pool_create(scratch_pool);

  old_external_urls = apr_hash_make(scratch_pool);

  /* Parse the old externals. This part will be replaced by reading EXTERNALS
     from the DB. */
#ifdef USE_EXTERNALS_STORE
  {
    apr_hash_t *external_defs;
    SVN_ERR(svn_wc__externals_defined_below(&external_defs,
                                            ctx->wc_ctx, target_abspath,
                                            scratch_pool, iterpool));

    for (hi = apr_hash_first(scratch_pool, external_defs);
         hi;
         hi = apr_hash_next(hi))
      {
        const char *old_url;
        const char *item_abspath = svn__apr_hash_index_key(hi);

        /* This should be moved into the external processing code once
           the EXTERNALS store is in place */
        SVN_ERR(svn_wc__read_external_info(NULL, NULL, &old_url, NULL, NULL,
                                           ctx->wc_ctx, target_abspath,
                                           item_abspath, FALSE,
                                           scratch_pool, iterpool));

        apr_hash_set(old_external_urls, item_abspath,
                     APR_HASH_KEY_STRING, old_url);
      }
  }
#else
  if (externals_old)
    {
      for (hi = apr_hash_first(scratch_pool, externals_old);
           hi;
           hi = apr_hash_next(hi))
        {
          const char *old_desc_text;
          apr_array_header_t *old_desc;
          const char *def_abspath;
          const char *url;
          int i;

          svn_pool_clear(iterpool);

          def_abspath = svn__apr_hash_index_key(hi);
          old_desc_text = svn__apr_hash_index_val(hi);

          SVN_ERR(svn_wc_parse_externals_description3(&old_desc, def_abspath,
                                                      old_desc_text,
                                                      FALSE, iterpool));

          SVN_ERR(svn_wc__node_get_url(&url, ctx->wc_ctx, def_abspath,
                                       iterpool, iterpool));

          SVN_ERR_ASSERT(url);

          for (i = 0; old_desc && (i < old_desc->nelts); i++)
            {
              svn_wc_external_item2_t *item;
              const char *item_abspath;
              const char *old_url;

              item = APR_ARRAY_IDX(old_desc, i, svn_wc_external_item2_t *);
              item_abspath = svn_dirent_join(def_abspath, item->target_dir,
                                               scratch_pool);

              SVN_ERR(resolve_relative_external_url(&old_url, item,
                                                    repos_root_url,
                                                    url, scratch_pool,
                                                    iterpool));

              apr_hash_set(old_external_urls, item_abspath,
                           APR_HASH_KEY_STRING, old_url);
            }
         }
    }
#endif

  for (hi = apr_hash_first(scratch_pool, combined);
       hi;
       hi = apr_hash_next(hi))
    {
      const void *key;
      apr_ssize_t klen;
      svn_depth_t ambient_depth = svn_depth_infinity;
      svn_pool_clear(iterpool);

      apr_hash_this(hi, &key, &klen, NULL);

      if (ambient_depths)
        {
          const char *ambient_depth_w;

          ambient_depth_w = apr_hash_get(ambient_depths, key, klen);

          if (ambient_depth_w == NULL)
            {
              return svn_error_createf(
                        SVN_ERR_WC_CORRUPT, NULL,
                        _("Traversal of '%s' found no ambient depth"),
                        svn_dirent_local_style(key, scratch_pool));
            }
          else
            {
              ambient_depth = svn_depth_from_word(ambient_depth_w);
            }
        }

      SVN_ERR(handle_externals_change(
                &eb,
                svn__apr_hash_index_key(hi),
                externals_new ? apr_hash_get(externals_new, key, klen) : NULL,
                old_external_urls,
                ambient_depth, requested_depth,
                iterpool));
    }

  /* Remove the remaining externals */
  for (hi = apr_hash_first(scratch_pool, old_external_urls);
       hi;
       hi = apr_hash_next(hi))
    {
      const char *item_abspath;
      const char *old_url;

      svn_pool_clear(iterpool);

      item_abspath = svn__apr_hash_index_key(hi);
      old_url = svn__apr_hash_index_val(hi);

      SVN_ERR(wrap_external_error(
                          &eb, item_abspath,
                          handle_external_item_removal(&eb, target_abspath,
                                                       item_abspath,
                                                       old_url, iterpool),
                          iterpool));
    }


  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}


svn_error_t *
svn_client__export_externals(apr_hash_t *externals,
                             const char *from_url,
                             const char *to_abspath,
                             const char *repos_root_url,
                             svn_depth_t requested_depth,
                             const char *native_eol,
                             svn_boolean_t *timestamp_sleep,
                             svn_client_ctx_t *ctx,
                             apr_pool_t *scratch_pool)
{
  struct external_change_baton_t eb = { 0 };
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  apr_pool_t *sub_iterpool = svn_pool_create(scratch_pool);
  apr_hash_index_t *hi;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(to_abspath));

  eb.repos_root_url = repos_root_url;
  eb.ctx = ctx;
  eb.native_eol = native_eol;
  eb.timestamp_sleep = timestamp_sleep;
  eb.is_export = TRUE;

  for (hi = apr_hash_first(scratch_pool, externals);
       hi;
       hi = apr_hash_next(hi))
    {
      const char *local_abspath = svn__apr_hash_index_key(hi);
      const char *desc_text = svn__apr_hash_index_val(hi);
      const char *local_relpath;
      const char *dir_url;
      apr_array_header_t *items;
      int i;

      svn_pool_clear(iterpool);

      SVN_ERR(svn_wc_parse_externals_description3(&items, local_abspath,
                                                  desc_text, FALSE,
                                                  scratch_pool));

      if (! items->nelts)
        continue;

      local_relpath = svn_dirent_skip_ancestor(to_abspath, local_abspath);

      dir_url = svn_path_url_add_component2(from_url, local_relpath,
                                            scratch_pool);

      for (i = 0; i < items->nelts; i++)
        {
          const char *item_abspath;
          svn_wc_external_item2_t *item = APR_ARRAY_IDX(items, i,
                                                svn_wc_external_item2_t *);

          svn_pool_clear(sub_iterpool);

          item_abspath = svn_dirent_join(local_abspath, item->target_dir,
                                         sub_iterpool);

          SVN_ERR(wrap_external_error(
                          &eb, item_abspath,
                          handle_external_item_change(&eb, local_abspath,
                                                      dir_url, item_abspath,
                                                      NULL, item,
                                                      sub_iterpool),
                          sub_iterpool));
        }
    }

  svn_pool_destroy(sub_iterpool);
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_client__do_external_status(svn_client_ctx_t *ctx,
                               apr_hash_t *externals_new,
                               svn_depth_t depth,
                               svn_boolean_t get_all,
                               svn_boolean_t update,
                               svn_boolean_t no_ignore,
                               svn_client_status_func_t status_func,
                               void *status_baton,
                               apr_pool_t *pool)
{
  apr_hash_index_t *hi;
  apr_pool_t *subpool = svn_pool_create(pool);

  /* Loop over the hash of new values (we don't care about the old
     ones).  This is a mapping of versioned directories to property
     values. */
  for (hi = apr_hash_first(pool, externals_new);
       hi;
       hi = apr_hash_next(hi))
    {
      apr_array_header_t *exts;
      const char *path = svn__apr_hash_index_key(hi);
      const char *propval = svn__apr_hash_index_val(hi);
      apr_pool_t *iterpool;
      int i;

      /* Clear the subpool. */
      svn_pool_clear(subpool);

      /* Parse the svn:externals property value.  This results in a
         hash mapping subdirectories to externals structures. */
      SVN_ERR(svn_wc_parse_externals_description3(&exts, path, propval,
                                                  FALSE, subpool));

      /* Make a sub-pool of SUBPOOL. */
      iterpool = svn_pool_create(subpool);

      /* Loop over the subdir array. */
      for (i = 0; exts && (i < exts->nelts); i++)
        {
          const char *fullpath;
          svn_wc_external_item2_t *external;
          svn_node_kind_t kind;

          svn_pool_clear(iterpool);

          external = APR_ARRAY_IDX(exts, i, svn_wc_external_item2_t *);
          fullpath = svn_dirent_join(path, external->target_dir, iterpool);

          /* If the external target directory doesn't exist on disk,
             just skip it. */
          SVN_ERR(svn_io_check_path(fullpath, &kind, iterpool));
          if (kind != svn_node_dir)
            continue;

          /* Tell the client we're starting an external status set. */
          if (ctx->notify_func2)
            (ctx->notify_func2)
              (ctx->notify_baton2,
               svn_wc_create_notify(fullpath, svn_wc_notify_status_external,
                                    iterpool), iterpool);

          /* And then do the status. */
          SVN_ERR(svn_client_status5(NULL, ctx, fullpath,
                                     &(external->revision),
                                     depth, get_all, update,
                                     no_ignore, FALSE, FALSE, NULL,
                                     status_func, status_baton,
                                     iterpool));
        }
    }

  /* Destroy SUBPOOL and (implicitly) ITERPOOL. */
  svn_pool_destroy(subpool);

  return SVN_NO_ERROR;
}


/* Implements the `svn_wc_externals_update_t' interface. */
svn_error_t *
svn_client__external_info_gatherer(void *baton,
                                   const char *local_abspath,
                                   const svn_string_t *old_value,
                                   const svn_string_t *new_value,
                                   svn_depth_t depth,
                                   apr_pool_t *scratch_pool)
{
  svn_client__external_func_baton_t *efb = baton;

  local_abspath = apr_pstrdup(efb->result_pool, local_abspath);

  if (efb->externals_old != NULL && old_value != NULL)
    apr_hash_set(efb->externals_old, local_abspath, APR_HASH_KEY_STRING,
                 apr_pstrndup(efb->result_pool,
                              old_value->data, old_value->len));

  if (efb->externals_new != NULL && new_value != NULL)
    apr_hash_set(efb->externals_new, local_abspath, APR_HASH_KEY_STRING,
                 apr_pstrndup(efb->result_pool,
                              new_value->data, new_value->len));

  if (efb->ambient_depths != NULL)
    apr_hash_set(efb->ambient_depths, local_abspath, APR_HASH_KEY_STRING,
                 svn_depth_to_word(depth));

  return SVN_NO_ERROR;
}


/* An implementation of svn_wc__proplist_receiver_t. Just squirrels away an
   svn:externals property value into BATON (which is an apr_hash_t *
   keyed on local absolute path).  */
static svn_error_t *
externals_crawl_proplist_receiver(void *baton,
                                  const char *local_abspath,
                                  apr_hash_t *props,
                                  apr_pool_t *scratch_pool)
{
  apr_hash_t *externals_hash = baton;
  const svn_string_t *propval;

  propval = apr_hash_get(props, SVN_PROP_EXTERNALS, APR_HASH_KEY_STRING);

  if (propval)
    {
      apr_pool_t *hash_pool = apr_hash_pool_get(externals_hash);

      apr_hash_set(externals_hash, apr_pstrdup(hash_pool, local_abspath),
                   APR_HASH_KEY_STRING, svn_string_dup(propval, hash_pool));
    }

  return SVN_NO_ERROR;
}


svn_error_t *
svn_client__crawl_for_externals(apr_hash_t **externals_p,
                                const char *local_abspath,
                                svn_depth_t depth,
                                svn_client_ctx_t *ctx,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  apr_hash_t *externals_hash = apr_hash_make(result_pool);

  SVN_ERR(svn_wc__prop_list_recursive(ctx->wc_ctx, local_abspath, NULL, depth,
                                      FALSE /* base_props */,
                                      FALSE /* pristine */,
                                      externals_crawl_proplist_receiver,
                                      externals_hash,
                                      ctx->cancel_func,
                                      ctx->cancel_baton,
                                      scratch_pool));

  *externals_p = externals_hash;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__gather_local_external_changes(apr_hash_t *externals_new,
                                          apr_hash_t *ambient_depths,
                                          const char *anchor_abspath,
                                          svn_depth_t requested_depth,
                                          svn_client_ctx_t *ctx,
                                          apr_pool_t *scratch_pool)
{
  apr_hash_t *all_externals;
  apr_hash_index_t *hi;

  /* If there was no requested depth for this operation, use infinity.
   * svn_client__crawl_for_externals() doesn't like depth 'unknown'. */
  if (requested_depth == svn_depth_unknown)
    requested_depth = svn_depth_infinity;

  SVN_ERR(svn_client__crawl_for_externals(&all_externals, anchor_abspath,
                                          requested_depth, ctx, scratch_pool,
                                          scratch_pool));

  for (hi = apr_hash_first(scratch_pool, all_externals);
       hi;
       hi = apr_hash_next(hi))
    {
      const char *local_abspath = svn__apr_hash_index_key(hi);
      svn_string_t *propval = svn__apr_hash_index_val(hi);
      apr_pool_t *hash_pool = apr_hash_pool_get(externals_new);

      local_abspath = apr_pstrdup(hash_pool, local_abspath);

      /* Override existing pristine definitions */
      apr_hash_set(externals_new, local_abspath, APR_HASH_KEY_STRING,
                   apr_pstrdup(hash_pool, propval->data));

      /* Make sure that when using ambient depths, there is a depth for
         every path */
      if (ambient_depths
          && !apr_hash_get(ambient_depths, local_abspath, APR_HASH_KEY_STRING))
        {
          apr_hash_set(ambient_depths, local_abspath, APR_HASH_KEY_STRING,
                       svn_depth_to_word(svn_depth_infinity));
        }
    }

  return SVN_NO_ERROR;
}


