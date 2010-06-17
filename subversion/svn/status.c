/*
 * status.c:  the command-line's portion of the "svn status" command
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
#include "svn_cmdline.h"
#include "svn_wc.h"
#include "svn_dirent_uri.h"
#include "svn_xml.h"
#include "svn_time.h"
#include "cl.h"
#include "svn_private_config.h"
#include "tree-conflicts.h"
#include "private/svn_wc_private.h"

/* Return the single character representation of STATUS */
static char
generate_status_code(enum svn_wc_status_kind status)
{
  switch (status)
    {
    case svn_wc_status_none:        return ' ';
    case svn_wc_status_normal:      return ' ';
    case svn_wc_status_added:       return 'A';
    case svn_wc_status_missing:     return '!';
    case svn_wc_status_incomplete:  return '!';
    case svn_wc_status_deleted:     return 'D';
    case svn_wc_status_replaced:    return 'R';
    case svn_wc_status_modified:    return 'M';
    case svn_wc_status_merged:      return 'G';
    case svn_wc_status_conflicted:  return 'C';
    case svn_wc_status_obstructed:  return '~';
    case svn_wc_status_ignored:     return 'I';
    case svn_wc_status_external:    return 'X';
    case svn_wc_status_unversioned: return '?';
    default:                        return '?';
    }
}

/* Return the single character representation of the switched column
   status. */
static char
generate_switch_column_code(const svn_client_status_t *status)
{
  if (status->switched)
    return 'S';
  else if (status->file_external)
    return 'X';
  else
    return ' ';
}

/* Return the detailed string representation of STATUS */
static const char *
generate_status_desc(enum svn_wc_status_kind status)
{
  switch (status)
    {
    case svn_wc_status_none:        return "none";
    case svn_wc_status_normal:      return "normal";
    case svn_wc_status_added:       return "added";
    case svn_wc_status_missing:     return "missing";
    case svn_wc_status_incomplete:  return "incomplete";
    case svn_wc_status_deleted:     return "deleted";
    case svn_wc_status_replaced:    return "replaced";
    case svn_wc_status_modified:    return "modified";
    case svn_wc_status_merged:      return "merged";
    case svn_wc_status_conflicted:  return "conflicted";
    case svn_wc_status_obstructed:  return "obstructed";
    case svn_wc_status_ignored:     return "ignored";
    case svn_wc_status_external:    return "external";
    case svn_wc_status_unversioned: return "unversioned";
    default:                        abort();
    }
}


/* Print STATUS and PATH in a format determined by DETAILED and
   SHOW_LAST_COMMITTED. */
static svn_error_t *
print_status(const char *path,
             svn_boolean_t detailed,
             svn_boolean_t show_last_committed,
             svn_boolean_t repos_locks,
             const svn_client_status_t *status,
             unsigned int *text_conflicts,
             unsigned int *prop_conflicts,
             unsigned int *tree_conflicts,
             svn_client_ctx_t *ctx,
             apr_pool_t *pool)
{
  enum svn_wc_status_kind text_status = status->text_status;
  char tree_status_code = ' ';
  const char *tree_desc_line = "";

  /* To indicate this node is the victim of a tree conflict, we show
     'C' in the tree-conflict column, overriding any other status.
     We also print a separate line describing the nature of the tree
     conflict. */
  if (status->conflicted)
    {
      const char *desc;
      const char *local_abspath;
      svn_boolean_t text_conflicted;
      svn_boolean_t prop_conflicted;
      svn_boolean_t tree_conflicted;

      SVN_ERR(svn_dirent_get_absolute(&local_abspath, path, pool));

      SVN_ERR(svn_wc__node_check_conflicts(&prop_conflicted,
                                           &text_conflicted,
                                           &tree_conflicted, ctx->wc_ctx,
                                           local_abspath, pool, pool));


      if (tree_conflicted)
        {
          const svn_wc_conflict_description2_t *tree_conflict;
          svn_wc_conflict_description_t *old_tree_conflict;
          SVN_ERR(svn_wc__get_tree_conflict(&tree_conflict, ctx->wc_ctx,
                                            local_abspath, pool, pool));
          old_tree_conflict = svn_wc__cd2_to_cd(tree_conflict, pool);

          tree_status_code = 'C';
          svn_cl__get_human_readable_tree_conflict_description(
            &desc, old_tree_conflict, pool);
          tree_desc_line = apr_psprintf(pool, "\n      >   %s", desc);
          (*tree_conflicts)++;
        }
      else if (text_conflicted)
        (*text_conflicts)++;
      else if (prop_conflicted)
        (*prop_conflicts)++;
    }

  if (detailed)
    {
      char ood_status, lock_status;
      const char *working_rev;

      /* ### FIXME: For now, we'll tweak an SVN_INVALID_REVNUM and make it
         ### 0. In WC-1, files scheduled for addition were assigned
         ### revision=0.  This is wrong, and we're trying to remedy that,
         ### but for the sake of test suite and code sanity now in WC-NG,
         ### we'll just maintain the old behavior. */
      if (! status->versioned)
        working_rev = "";
      else if (! SVN_IS_VALID_REVNUM(status->revision))
        {
          if (status->copied)
            working_rev = "-";
          else if (text_status == svn_wc_status_added
              || text_status == svn_wc_status_replaced)
            working_rev = "0";
          else
            working_rev = " ? ";
        }
      else if (status->copied)
        working_rev = "-";
      else
        working_rev = apr_psprintf(pool, "%ld", status->revision);

      if (status->repos_text_status != svn_wc_status_none
          || status->repos_prop_status != svn_wc_status_none)
        ood_status = '*';
      else
        ood_status = ' ';

      if (repos_locks)
        {
          if (status->repos_lock)
            {
              if (status->lock)
                {
                  if (strcmp(status->repos_lock->token, status->lock->token)
                      == 0)
                    lock_status = 'K';
                  else
                    lock_status = 'T';
                }
              else
                lock_status = 'O';
            }
          else if (status->lock)
            lock_status = 'B';
          else
            lock_status = ' ';
        }
      else
        lock_status = (status->lock) ? 'K' : ' ';

      if (show_last_committed)
        {
          const char *commit_rev;
          const char *commit_author;

          if (SVN_IS_VALID_REVNUM(status->changed_rev))
            commit_rev = apr_psprintf(pool, "%ld", status->changed_rev);
          else if (status->versioned)
            commit_rev = " ? ";
          else
            commit_rev = "";

          if (status->changed_author)
            commit_author = status->changed_author;
          else if (status->versioned)
            commit_author = " ? ";
          else
            commit_author = "";

          SVN_ERR
            (svn_cmdline_printf(pool,
                                "%c%c%c%c%c%c%c %c   %6s   %6s %-12s %s%s\n",
                                generate_status_code(text_status),
                                generate_status_code(status->prop_status),
                                status->locked ? 'L' : ' ',
                                status->copied ? '+' : ' ',
                                generate_switch_column_code(status),
                                lock_status,
                                tree_status_code,
                                ood_status,
                                working_rev,
                                commit_rev,
                                commit_author,
                                path,
                                tree_desc_line));
        }
      else
        SVN_ERR
          (svn_cmdline_printf(pool, "%c%c%c%c%c%c%c %c   %6s   %s%s\n",
                              generate_status_code(text_status),
                              generate_status_code(status->prop_status),
                              status->locked ? 'L' : ' ',
                              status->copied ? '+' : ' ',
                              generate_switch_column_code(status),
                              lock_status,
                              tree_status_code,
                              ood_status,
                              working_rev,
                              path,
                              tree_desc_line));
    }
  else
    SVN_ERR
      (svn_cmdline_printf(pool, "%c%c%c%c%c%c%c %s%s\n",
                          generate_status_code(text_status),
                          generate_status_code(status->prop_status),
                          status->locked ? 'L' : ' ',
                          status->copied ? '+' : ' ',
                          generate_switch_column_code(status),
                          ((status->lock)
                           ? 'K' : ' '),
                          tree_status_code,
                          path,
                          tree_desc_line));

  return svn_cmdline_fflush(stdout);
}


svn_error_t *
svn_cl__print_status_xml(const char *path,
                         const svn_client_status_t *status,
                         svn_client_ctx_t *ctx,
                         apr_pool_t *pool)
{
  svn_stringbuf_t *sb = svn_stringbuf_create("", pool);
  apr_hash_t *att_hash;
  const char *local_abspath;
  svn_boolean_t tree_conflicted;

  if (status->text_status == svn_wc_status_none
      && status->repos_text_status == svn_wc_status_none)
    return SVN_NO_ERROR;

  SVN_ERR(svn_dirent_get_absolute(&local_abspath, path, pool));
  SVN_ERR(svn_wc__node_check_conflicts(NULL, NULL, &tree_conflicted,
                                       ctx->wc_ctx, local_abspath, pool,
                                       pool));

  svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "entry",
                        "path", svn_dirent_local_style(path, pool), NULL);

  att_hash = apr_hash_make(pool);
  apr_hash_set(att_hash, "item", APR_HASH_KEY_STRING,
               generate_status_desc(status->text_status));
  apr_hash_set(att_hash, "props", APR_HASH_KEY_STRING,
               generate_status_desc(status->prop_status));
  if (status->locked)
    apr_hash_set(att_hash, "wc-locked", APR_HASH_KEY_STRING, "true");
  if (status->copied)
    apr_hash_set(att_hash, "copied", APR_HASH_KEY_STRING, "true");
  if (status->switched)
    apr_hash_set(att_hash, "switched", APR_HASH_KEY_STRING, "true");
  if (status->file_external)
    apr_hash_set(att_hash, "file-external", APR_HASH_KEY_STRING, "true");
  if (status->versioned && ! status->copied)
    apr_hash_set(att_hash, "revision", APR_HASH_KEY_STRING,
                 apr_psprintf(pool, "%ld", status->revision));
  if (tree_conflicted)
    apr_hash_set(att_hash, "tree-conflicted", APR_HASH_KEY_STRING,
                 "true");
  svn_xml_make_open_tag_hash(&sb, pool, svn_xml_normal, "wc-status",
                             att_hash);

  if (SVN_IS_VALID_REVNUM(status->changed_rev))
    {
      svn_cl__print_xml_commit(&sb, status->changed_rev,
                               status->changed_author,
                               svn_time_to_cstring(status->changed_date,
                                                   pool),
                               pool);
    }

  if (status->lock)
    {
      svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "lock", NULL);

      svn_cl__xml_tagged_cdata(&sb, pool, "token", status->lock->token);

      /* If lock_owner is NULL, assume WC is corrupt. */
      if (status->lock->owner)
        svn_cl__xml_tagged_cdata(&sb, pool, "owner",
                                 status->lock->owner);
      else
        return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
                                 _("'%s' has lock token, but no lock owner"),
                                 svn_dirent_local_style(path, pool));

      svn_cl__xml_tagged_cdata(&sb, pool, "comment",
                               status->lock->comment);

      svn_cl__xml_tagged_cdata(&sb, pool, "created",
                               svn_time_to_cstring
                               (status->lock->creation_date, pool));

      svn_xml_make_close_tag(&sb, pool, "lock");
    }

  svn_xml_make_close_tag(&sb, pool, "wc-status");

  if (status->repos_text_status != svn_wc_status_none
      || status->repos_prop_status != svn_wc_status_none
      || status->repos_lock)
    {
      svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "repos-status",
                            "item",
                            generate_status_desc(status->repos_text_status),
                            "props",
                            generate_status_desc(status->repos_prop_status),
                            NULL);
      if (status->repos_lock)
        {
          svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "lock", NULL);

          svn_cl__xml_tagged_cdata(&sb, pool, "token",
                                   status->repos_lock->token);

          svn_cl__xml_tagged_cdata(&sb, pool, "owner",
                                   status->repos_lock->owner);

          svn_cl__xml_tagged_cdata(&sb, pool, "comment",
                                   status->repos_lock->comment);

          svn_cl__xml_tagged_cdata(&sb, pool, "created",
                                   svn_time_to_cstring
                                   (status->repos_lock->creation_date,
                                    pool));

          if (status->repos_lock->expiration_date != 0)
            {
              svn_cl__xml_tagged_cdata(&sb, pool, "expires",
                                       svn_time_to_cstring
                                       (status->repos_lock->expiration_date,
                                        pool));
            }

          svn_xml_make_close_tag(&sb, pool, "lock");
        }
      svn_xml_make_close_tag(&sb, pool, "repos-status");
    }

  svn_xml_make_close_tag(&sb, pool, "entry");

  return svn_cl__error_checked_fputs(sb->data, stdout);
}

/* Called by status-cmd.c */
svn_error_t *
svn_cl__print_status(const char *path,
                     const svn_client_status_t *status,
                     svn_boolean_t detailed,
                     svn_boolean_t show_last_committed,
                     svn_boolean_t skip_unrecognized,
                     svn_boolean_t repos_locks,
                     unsigned int *text_conflicts,
                     unsigned int *prop_conflicts,
                     unsigned int *tree_conflicts,
                     svn_client_ctx_t *ctx,
                     apr_pool_t *pool)
{
  const char *local_abspath = status->local_abspath;
  svn_boolean_t tree_conflicted;

  SVN_ERR(svn_wc__node_check_conflicts(NULL, NULL, &tree_conflicted,
                                       ctx->wc_ctx, local_abspath, pool,
                                       pool));
  if (! status
      || (skip_unrecognized && !(status->versioned || tree_conflicted))
      || (status->text_status == svn_wc_status_none
          && status->repos_text_status == svn_wc_status_none))
    return SVN_NO_ERROR;

  return print_status(svn_dirent_local_style(path, pool),
                      detailed, show_last_committed, repos_locks, status,
                      text_conflicts, prop_conflicts, tree_conflicts,
                      ctx, pool);
}
