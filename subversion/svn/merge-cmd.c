/*
 * merge-cmd.c -- Merging changes into a working copy.
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

#include "svn_client.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_error.h"
#include "svn_types.h"
#include "cl.h"
#include "private/svn_client_private.h"

#include "svn_private_config.h"


/*** Code. ***/

/* Do a reintegrate merge from SOURCE_PATH_OR_URL@SOURCE_PEG_REVISION into
 * TARGET_WCPATH.  Do it with a WC write lock unless DRY_RUN is true. */
static svn_error_t *
merge_reintegrate(const char *source_path_or_url,
                  const svn_opt_revision_t *source_peg_revision,
                  const char *target_wcpath,
                  svn_boolean_t dry_run,
                  const apr_array_header_t *merge_options,
                  svn_client_ctx_t *ctx,
                  apr_pool_t *scratch_pool)
{
  const char *url1, *url2;
  svn_revnum_t rev1, rev2;

  SVN_ERR(svn_client_find_reintegrate_merge(
            &url1, &rev1, &url2, &rev2,
            source_path_or_url, source_peg_revision, target_wcpath,
            ctx, scratch_pool, scratch_pool));

  if (url1)
    {
      svn_opt_revision_t revision1;
      svn_opt_revision_t revision2;

      revision1.kind = svn_opt_revision_number;
      revision1.value.number = rev1;

      revision2.kind = svn_opt_revision_number;
      revision2.value.number = rev2;

      /* Do the merge.  Set 'allow_mixed_rev' to true, not because we want
       * to allow a mixed-rev WC but simply to bypass the check, as it was
       * already checked in svn_client_find_reintegrate_merge(). */
      SVN_ERR(svn_client_merge4(url1, &revision1, url2, &revision2,
                                target_wcpath, svn_depth_infinity,
                                FALSE /* ignore_ancestry */,
                                FALSE /* force */,
                                FALSE /* record_only */,
                                dry_run, TRUE /* allow_mixed_rev */,
                                merge_options, ctx, scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* Throw an error if PATH_OR_URL is a path and REVISION isn't a repository
 * revision. */
static svn_error_t *
ensure_wc_path_has_repo_revision(const char *path_or_url,
                                 const svn_opt_revision_t *revision,
                                 apr_pool_t *scratch_pool)
{
  if (revision->kind != svn_opt_revision_number
      && revision->kind != svn_opt_revision_date
      && revision->kind != svn_opt_revision_head
      && ! svn_path_is_url(path_or_url))
    return svn_error_createf(
      SVN_ERR_CLIENT_BAD_REVISION, NULL,
      _("Invalid merge source '%s'; a working copy path can only be "
        "used with a repository revision (a number, a date, or head)"),
      svn_dirent_local_style(path_or_url, scratch_pool));
  return SVN_NO_ERROR;
}

/* Automatic, merge-tracking merge, used for sync or reintegrate purposes. */
static svn_error_t *
automatic_merge(const char *source_path_or_url,
                const svn_opt_revision_t *source_revision,
                const char *target_wcpath,
                svn_depth_t depth,
                svn_boolean_t force,
                svn_boolean_t record_only,
                svn_boolean_t dry_run,
                svn_boolean_t allow_mixed_rev,
                svn_boolean_t allow_local_mods,
                svn_boolean_t allow_switched_subtrees,
                const apr_array_header_t *merge_options,
                svn_client_ctx_t *ctx,
                apr_pool_t *scratch_pool)
{
  svn_client_automatic_merge_t *merge;

  /* Find the 3-way merges needed (and check suitability of the WC). */
  SVN_ERR(svn_client_find_automatic_merge(&merge,
                                          source_path_or_url, source_revision,
                                          target_wcpath, allow_mixed_rev,
                                          allow_local_mods, allow_switched_subtrees,
                                          ctx, scratch_pool, scratch_pool));

  if (svn_client_automatic_merge_is_reintegrate_like(merge))
    {
      if (record_only)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("The required merge is reintegrate-like, "
                                  "and the --record-only option "
                                  "cannot be used with this kind of merge"));

      if (depth != svn_depth_unknown)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("The required merge is reintegrate-like, "
                                  "and the --depth option "
                                  "cannot be used with this kind of merge"));

      if (force)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("The required merge is reintegrate-like, "
                                  "and the --force option "
                                  "cannot be used with this kind of merge"));

      if (allow_mixed_rev)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("The required merge is reintegrate-like, "
                                  "and the --allow-mixed-revisions option "
                                  "cannot be used with this kind of merge"));
    }

  /* Perform the 3-way merges */
  SVN_ERR(svn_client_do_automatic_merge(merge, target_wcpath, depth,
                                        force, record_only,
                                        dry_run, merge_options,
                                        ctx, scratch_pool));

  return SVN_NO_ERROR;
}

/* This implements the `svn_opt_subcommand_t' interface. */
svn_error_t *
svn_cl__merge(apr_getopt_t *os,
              void *baton,
              apr_pool_t *pool)
{
  svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
  svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
  apr_array_header_t *targets;
  const char *sourcepath1 = NULL, *sourcepath2 = NULL, *targetpath = "";
  svn_boolean_t two_sources_specified = TRUE;
  svn_error_t *err = SVN_NO_ERROR;
  svn_error_t *merge_err = SVN_NO_ERROR;
  svn_opt_revision_t first_range_start, first_range_end, peg_revision1,
    peg_revision2;
  apr_array_header_t *options, *ranges_to_merge = opt_state->revision_ranges;
  svn_opt_revision_t unspecified = { svn_opt_revision_unspecified, { 0 } };

  /* Merge doesn't support specifying a revision or revision range
     when using --reintegrate. */
  if (opt_state->reintegrate
      && opt_state->start_revision.kind != svn_opt_revision_unspecified)
    {
      return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                              _("-r and -c can't be used with --reintegrate"));
    }

  SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
                                                      opt_state->targets,
                                                      ctx, FALSE, pool));

  /* For now, we require at least one source.  That may change in
     future versions of Subversion, for example if we have support for
     negated mergeinfo.  See this IRC conversation:

       <bhuvan>   kfogel: yeah, i think you are correct; we should
                  specify the source url

       <kfogel>   bhuvan: I'll change the help output and propose for
                  backport.  Thanks.

       <bhuvan>   kfogel: np; while we are at it, 'svn merge' simply
                  returns nothing; i think we should say: """svn: Not
                  enough arguments provided; try 'svn help' for more
                  info"""

       <kfogel>   good idea

       <kfogel>   (in the future, 'svn merge' might actually do
                  something, but that's all the more reason to make
                  sure it errors now)

       <cmpilato> actually, i'm pretty sure 'svn merge' does something

       <cmpilato> it says "please merge any unmerged changes from
                  myself to myself."

       <cmpilato> :-)

       <kfogel>   har har

       <cmpilato> kfogel: i was serious.

       <kfogel>   cmpilato: urrr, uh.  Is that meaningful?  Is there
                  ever a reason for a user to run it?

       <cmpilato> kfogel: not while we don't have support for negated
                  mergeinfo.

       <kfogel>   cmpilato: do you concur that until it does something
                  useful it should error?

       <cmpilato> kfogel: yup.

       <kfogel>   cool
  */
  if (targets->nelts < 1)
    {
      return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0,
                              _("Merge source required"));
    }
  else  /* Parse at least one, and possible two, sources. */
    {
      SVN_ERR(svn_opt_parse_path(&peg_revision1, &sourcepath1,
                                 APR_ARRAY_IDX(targets, 0, const char *),
                                 pool));
      if (targets->nelts >= 2)
        SVN_ERR(svn_opt_parse_path(&peg_revision2, &sourcepath2,
                                   APR_ARRAY_IDX(targets, 1, const char *),
                                   pool));
    }

  /* We could have one or two sources.  Deliberately written to stay
     correct even if we someday permit implied merge source. */
  if (targets->nelts <= 1)
    {
      two_sources_specified = FALSE;
    }
  else if (targets->nelts == 2)
    {
      if (svn_path_is_url(sourcepath1) && !svn_path_is_url(sourcepath2))
        two_sources_specified = FALSE;
    }

  if (opt_state->revision_ranges->nelts > 0)
    {
      first_range_start = APR_ARRAY_IDX(opt_state->revision_ranges, 0,
                                        svn_opt_revision_range_t *)->start;
      first_range_end = APR_ARRAY_IDX(opt_state->revision_ranges, 0,
                                      svn_opt_revision_range_t *)->end;
    }
  else
    {
      first_range_start.kind = first_range_end.kind =
        svn_opt_revision_unspecified;
    }

  /* If revision_ranges has at least one real range at this point, then
     we know the user must have used the '-r' and/or '-c' switch(es).
     This means we're *not* doing two distinct sources. */
  if (first_range_start.kind != svn_opt_revision_unspecified)
    {
      /* A revision *range* is required. */
      if (first_range_end.kind == svn_opt_revision_unspecified)
        return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0,
                                _("Second revision required"));

      two_sources_specified = FALSE;
    }

  if (! two_sources_specified) /* TODO: Switch order of if */
    {
      if (targets->nelts > 2)
        return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                _("Too many arguments given"));

      /* Set the default value for unspecified paths and peg revision. */
      /* targets->nelts is 1 ("svn merge SOURCE") or 2 ("svn merge
         SOURCE WCPATH") here. */
      sourcepath2 = sourcepath1;

      if (peg_revision1.kind == svn_opt_revision_unspecified)
        peg_revision1.kind = svn_path_is_url(sourcepath1)
          ? svn_opt_revision_head : svn_opt_revision_working;

      if (targets->nelts == 2)
        {
          targetpath = APR_ARRAY_IDX(targets, 1, const char *);
          if (svn_path_is_url(targetpath))
            return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                    _("Cannot specify a revision range "
                                      "with two URLs"));
        }
    }
  else /* using @rev syntax */
    {
      if (targets->nelts < 2)
        return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, NULL, NULL);
      if (targets->nelts > 3)
        return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                _("Too many arguments given"));

      first_range_start = peg_revision1;
      first_range_end = peg_revision2;

      /* Catch 'svn merge wc_path1 wc_path2 [target]' without explicit
         revisions--since it ignores local modifications it may not do what
         the user expects.  That is, it doesn't read from the WC itself, it
         reads from the WC's URL.  Forcing the user to specify a repository
         revision should avoid any confusion. */
      SVN_ERR(ensure_wc_path_has_repo_revision(sourcepath1, &first_range_start,
                                               pool));
      SVN_ERR(ensure_wc_path_has_repo_revision(sourcepath2, &first_range_end,
                                               pool));

      /* Default peg revisions to each URL's youngest revision. */
      if (first_range_start.kind == svn_opt_revision_unspecified)
        first_range_start.kind = svn_opt_revision_head;
      if (first_range_end.kind == svn_opt_revision_unspecified)
        first_range_end.kind = svn_opt_revision_head;

      /* Decide where to apply the delta (defaulting to "."). */
      if (targets->nelts == 3)
        targetpath = APR_ARRAY_IDX(targets, 2, const char *);
    }

  /* If no targetpath was specified, see if we can infer it from the
     sourcepaths. */
  if (sourcepath1 && sourcepath2 && strcmp(targetpath, "") == 0)
    {
      /* If the sourcepath is a URL, it can only refer to a target in
         the current working directory.  However, if the sourcepath is
         a local path, it can refer to a target somewhere deeper in
         the directory structure. */
      if (svn_path_is_url(sourcepath1))
        {
          const char *sp1_basename = svn_uri_basename(sourcepath1, pool);
          const char *sp2_basename = svn_uri_basename(sourcepath2, pool);

          if (strcmp(sp1_basename, sp2_basename) == 0)
            {
              svn_node_kind_t kind;

              SVN_ERR(svn_io_check_path(sp1_basename, &kind, pool));
              if (kind == svn_node_file)
                {
                  targetpath = sp1_basename;
                }
            }
        }
      else if (strcmp(sourcepath1, sourcepath2) == 0)
        {
          svn_node_kind_t kind;

          SVN_ERR(svn_io_check_path(sourcepath1, &kind, pool));
          if (kind == svn_node_file)
            {
              targetpath = sourcepath1;
            }
        }
    }

  if (opt_state->extensions)
    options = svn_cstring_split(opt_state->extensions, " \t\n\r", TRUE, pool);
  else
    options = NULL;

  /* More input validation. */
  if (opt_state->reintegrate)
    {
      if (opt_state->ignore_ancestry)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("--reintegrate cannot be used with "
                                  "--ignore-ancestry"));

      if (opt_state->record_only)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("--reintegrate cannot be used with "
                                  "--record-only"));

      if (opt_state->depth != svn_depth_unknown)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("--depth cannot be used with "
                                  "--reintegrate"));

      if (opt_state->force)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("--force cannot be used with "
                                  "--reintegrate"));

      if (two_sources_specified)
        return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                _("--reintegrate can only be used with "
                                  "a single merge source"));
      if (opt_state->allow_mixed_rev)
        return svn_error_create(SVN_ERR_CL_MUTUALLY_EXCLUSIVE_ARGS, NULL,
                                _("--allow-mixed-revisions cannot be used "
                                  "with --reintegrate"));
    }

  /* Postpone conflict resolution during the merge operation.
   * If any conflicts occur we'll run the conflict resolver later. */

  /* Do an automatic merge if just one source and no revisions. */
  if ((! two_sources_specified)
      && (! opt_state->reintegrate)
      && (! opt_state->ignore_ancestry)
      && first_range_start.kind == svn_opt_revision_unspecified
      && first_range_end.kind == svn_opt_revision_unspecified)
    {
      SVN_ERR_W(svn_cl__check_related_source_and_target(
                  sourcepath1, &peg_revision1, targetpath, &unspecified,
                  ctx, pool),
                _("Source and target must be different but related branches"));

      merge_err = automatic_merge(sourcepath1, &peg_revision1, targetpath,
                                  opt_state->depth,
                                  opt_state->force,
                                  opt_state->record_only,
                                  opt_state->dry_run,
                                  opt_state->allow_mixed_rev,
                                  TRUE /*allow_local_mods*/,
                                  TRUE /*allow_switched_subtrees*/,
                                  options, ctx, pool);
    }
  else if (opt_state->reintegrate)
    {
      SVN_ERR_W(svn_cl__check_related_source_and_target(
                  sourcepath1, &peg_revision1, targetpath, &unspecified,
                  ctx, pool),
                _("Source and target must be different but related branches"));

      merge_err = merge_reintegrate(sourcepath1, &peg_revision1, targetpath,
                                    opt_state->dry_run, options, ctx, pool);
    }
  else if (! two_sources_specified)
    {
      /* If we don't have at least one valid revision range, pick a
         good one that spans the entire set of revisions on our
         source. */
      if ((first_range_start.kind == svn_opt_revision_unspecified)
          && (first_range_end.kind == svn_opt_revision_unspecified))
        {
          svn_opt_revision_range_t *range = apr_pcalloc(pool, sizeof(*range));
          ranges_to_merge = apr_array_make(pool, 1, sizeof(range));
          range->start.kind = svn_opt_revision_number;
          range->start.value.number = 1;
          range->end = peg_revision1;
          APR_ARRAY_PUSH(ranges_to_merge, svn_opt_revision_range_t *) = range;

          /* This must be a 'sync' merge so check branch relationship. */
          SVN_ERR_W(svn_cl__check_related_source_and_target(
                      sourcepath1, &peg_revision1, targetpath, &unspecified,
                      ctx, pool),
                _("Source and target must be different but related branches"));
        }

      merge_err = svn_client_merge_peg4(sourcepath1,
                                        ranges_to_merge,
                                        &peg_revision1,
                                        targetpath,
                                        opt_state->depth,
                                        opt_state->ignore_ancestry,
                                        opt_state->force,
                                        opt_state->record_only,
                                        opt_state->dry_run,
                                        opt_state->allow_mixed_rev,
                                        options,
                                        ctx,
                                        pool);
    }
  else
    {
      if (svn_path_is_url(sourcepath1) != svn_path_is_url(sourcepath2))
        return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                _("Merge sources must both be "
                                  "either paths or URLs"));
      merge_err = svn_client_merge4(sourcepath1,
                                    &first_range_start,
                                    sourcepath2,
                                    &first_range_end,
                                    targetpath,
                                    opt_state->depth,
                                    opt_state->ignore_ancestry,
                                    opt_state->force,
                                    opt_state->record_only,
                                    opt_state->dry_run,
                                    opt_state->allow_mixed_rev,
                                    options,
                                    ctx,
                                    pool);
    }

  if (! opt_state->quiet)
    err = svn_cl__print_conflict_stats(ctx->notify_baton2, pool);

  if (!err)
    err = svn_cl__resolve_postponed_conflicts(ctx->conflict_baton2,
                                              opt_state->depth,
                                              opt_state->accept_which,
                                              opt_state->editor_cmd,
                                              ctx, pool);
  if (merge_err)
    {
      if (merge_err->apr_err ==
          SVN_ERR_CLIENT_INVALID_MERGEINFO_NO_MERGETRACKING)
        {
          err = svn_error_quick_wrap(
            svn_error_compose_create(merge_err, err),
            _("Merge tracking not possible, use --ignore-ancestry or\n"
              "fix invalid mergeinfo in target with 'svn propset'"));
        }
      else
        {
          err = svn_error_compose_create(merge_err, err);
          return svn_cl__may_need_force(err);
        }
    }

  return svn_error_trace(err);
}
