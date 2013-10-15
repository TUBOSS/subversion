/*
 * main.c:  Subversion command line client.
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

#include <string.h>
#include <assert.h>

#include <apr_signal.h>

#include "svn_cmdline.h"
#include "svn_dirent_uri.h"
#include "svn_pools.h"
#include "svn_utf.h"
#include "svn_version.h"

#include "cl.h"

#include "private/svn_opt_private.h"
#include "private/svn_cmdline_private.h"

#include "svn_private_config.h"


/*** Option Processing ***/

/* Add an identifier here for long options that don't have a short
   option. Options that have both long and short options should just
   use the short option letter as identifier.  */
typedef enum svn_cl__longopt_t {
  opt_auth_password = SVN_OPT_FIRST_LONGOPT_ID,
  opt_auth_username,
  opt_config_dir,
  opt_config_options,
  opt_depth,
  opt_no_auth_cache,
  opt_non_interactive,
  opt_stop_on_copy,
  opt_strict,
  opt_targets,
  opt_version,
  opt_with_revprop,
  opt_with_all_revprops,
  opt_with_no_revprops,
  opt_auto_moves,
  opt_trust_server_cert,
  opt_changelist
} svn_cl__longopt_t;


/* Option codes and descriptions for the command line client.
 *
 * The entire list must be terminated with an entry of nulls.
 */
const apr_getopt_option_t svn_cl__options[] =
{
  {"help",          'h', 0, N_("show help on a subcommand")},
  {NULL,            '?', 0, N_("show help on a subcommand")},
  {"quiet",         'q', 0, N_("print nothing, or only summary information")},
  {"recursive",     'R', 0, N_("descend recursively, same as --depth=infinity")},
  {"non-recursive", 'N', 0, N_("obsolete; try --depth=files or --depth=immediates")},
  {"change",        'c', 1,
                    N_("the change made by revision ARG (like -r ARG-1:ARG)\n"
                       "                             "
                       "If ARG is negative this is like -r ARG:ARG-1\n"
                       "                             "
                       "If ARG is of the form ARG1-ARG2 then this is like\n"
                       "                             "
                       "ARG1:ARG2, where ARG1 is inclusive")},
  {"revision",      'r', 1,
                    N_("ARG (some commands also take ARG1:ARG2 range)\n"
                       "                             "
                       "A revision argument can be one of:\n"
                       "                             "
                       "   NUMBER       revision number\n"
                       "                             "
                       "   '{' DATE '}' revision at start of the date\n"
                       "                             "
                       "   'HEAD'       latest in repository\n"
                       "                             "
                       "   'BASE'       base rev of item's working copy\n"
                       "                             "
                       "   'COMMITTED'  last commit at or before BASE\n"
                       "                             "
                       "   'PREV'       revision just before COMMITTED")},
  {"version",       opt_version, 0, N_("show program version information")},
  {"verbose",       'v', 0, N_("print extra information")},
  {"username",      opt_auth_username, 1, N_("specify a username ARG")},
  {"password",      opt_auth_password, 1, N_("specify a password ARG")},
  {"targets",       opt_targets, 1,
                    N_("pass contents of file ARG as additional args")},
  {"depth",         opt_depth, 1,
                    N_("limit operation by depth ARG ('empty', 'files',\n"
                       "                             "
                       "'immediates', or 'infinity')")},
  {"strict",        opt_strict, 0, N_("use strict semantics")},
  {"stop-on-copy",  opt_stop_on_copy, 0,
                    N_("do not cross copies while traversing history")},
  {"no-auth-cache", opt_no_auth_cache, 0,
                    N_("do not cache authentication tokens")},
  {"trust-server-cert", opt_trust_server_cert, 0,
                    N_("accept SSL server certificates from unknown\n"
                       "                             "
                       "certificate authorities without prompting (but only\n"
                       "                             "
                       "with '--non-interactive')") },
  {"non-interactive", opt_non_interactive, 0,
                    N_("do no interactive prompting")},
  {"config-dir",    opt_config_dir, 1,
                    N_("read user configuration files from directory ARG")},
  {"config-option", opt_config_options, 1,
                    N_("set user configuration option in the format:\n"
                       "                             "
                       "    FILE:SECTION:OPTION=[VALUE]\n"
                       "                             "
                       "For example:\n"
                       "                             "
                       "    servers:global:http-library=serf")},
  {"limit",         'l', 1, N_("maximum number of log entries")},
  {"with-all-revprops",  opt_with_all_revprops, 0,
                    N_("retrieve all revision properties")},
  {"with-no-revprops",  opt_with_no_revprops, 0,
                    N_("retrieve no revision properties")},
  {"with-revprop",  opt_with_revprop, 1,
                    N_("set revision property ARG in new revision\n"
                       "                             "
                       "using the name[=value] format")},
  {"auto-moves",    opt_auto_moves, 0,
                    N_("attempt to interpret matching unique DEL+ADD\n"
                       "                             "
                       "pairs as moves")},
  {"use-merge-history", 'g', 0,
                    N_("use/display additional information from merge\n"
                       "                             "
                       "history")},

  /* Long-opt Aliases
   *
   * These have NULL desriptions, but an option code that matches some
   * other option (whose description should probably mention its aliases).
  */

  {0,               0, 0, 0},
};



/*** Command dispatch. ***/

/* Our array of available subcommands.
 *
 * The entire list must be terminated with an entry of nulls.
 *
 * In most of the help text "PATH" is used where a working copy path is
 * required, "URL" where a repository URL is required and "TARGET" when
 * either a path or a url can be used.  Hmm, should this be part of the
 * help text?
 */

/* Options that apply to all commands.  (While not every command may
   currently require authentication or be interactive, allowing every
   command to take these arguments allows scripts to just pass them
   willy-nilly to every invocation of 'svn') . */
const int svn_cl__global_options[] =
{ opt_auth_username, opt_auth_password, opt_no_auth_cache, opt_non_interactive,
  opt_trust_server_cert, opt_config_dir, opt_config_options, 0
};

const svn_opt_subcommand_desc2_t svn_cl__cmd_table[] =
{
  { "help", svn_cl__help, {"?", "h"}, N_
    ("Describe the usage of this program or its subcommands.\n"
     "usage: help [SUBCOMMAND...]\n"),
    {0} },
  /* This command is also invoked if we see option "--help", "-h" or "-?". */

  { "null-export", svn_cl__null_export, {0}, N_
    ("Create an unversioned copy of a tree.\n"
     "usage: null-export [-r REV] URL[@PEGREV]\n"
     "\n"
     "  Exports a clean directory tree from the repository specified by\n"
     "  URL, at revision REV if it is given, otherwise at HEAD.\n"
     "\n"
     "  If specified, PEGREV determines in which revision the target is first\n"
     "  looked up.\n"),
    {'r', 'q', 'N', opt_depth} },

  { "null-list", svn_cl__null_list, {"ls"}, N_
    ("List directory entries in the repository.\n"
     "usage: list [TARGET[@REV]...]\n"
     "\n"
     "  List each TARGET file and the contents of each TARGET directory as\n"
     "  they exist in the repository.  If TARGET is a working copy path, the\n"
     "  corresponding repository URL will be used. If specified, REV determines\n"
     "  in which revision the target is first looked up.\n"
     "\n"
     "  The default TARGET is '.', meaning the repository URL of the current\n"
     "  working directory.\n"
     "\n"
     "  With --verbose, the following fields will be fetched for each item:\n"
     "\n"
     "    Revision number of the last commit\n"
     "    Author of the last commit\n"
     "    If locked, the letter 'O'.  (Use 'svn info URL' to see details)\n"
     "    Size (in bytes)\n"
     "    Date and time of the last commit\n"),
    {'r', 'v', 'q', 'R', opt_depth} },

  { "null-log", svn_cl__null_log, {0}, N_
    ("Fetch the log messages for a set of revision(s) and/or path(s).\n"
     "usage: 1. null-log [PATH][@REV]\n"
     "       2. null-log URL[@REV] [PATH...]\n"
     "\n"
     "  1. Fetch the log messages for the URL corresponding to PATH\n"
     "     (default: '.'). If specified, REV is the revision in which the\n"
     "     URL is first looked up, and the default revision range is REV:1.\n"
     "     If REV is not specified, the default revision range is BASE:1,\n"
     "     since the URL might not exist in the HEAD revision.\n"
     "\n"
     "  2. Fetch the log messages for the PATHs (default: '.') under URL.\n"
     "     If specified, REV is the revision in which the URL is first\n"
     "     looked up, and the default revision range is REV:1; otherwise,\n"
     "     the URL is looked up in HEAD, and the default revision range is\n"
     "     HEAD:1.\n"
     "\n"
     "  Multiple '-c' or '-r' options may be specified (but not a\n"
     "  combination of '-c' and '-r' options), and mixing of forward and\n"
     "  reverse ranges is allowed.\n"
     "\n"
     "  With -v, also print all affected paths with each log message.\n"
     "  With -q, don't print the log message body itself (note that this is\n"
     "  compatible with -v).\n"
     "\n"
     "  Each log message is printed just once, even if more than one of the\n"
     "  affected paths for that revision were explicitly requested.  Logs\n"
     "  follow copy history by default.  Use --stop-on-copy to disable this\n"
     "  behavior, which can be useful for determining branchpoints.\n"),
    {'r', 'q', 'v', 'g', 'c', opt_targets, opt_stop_on_copy,
     'l', opt_with_all_revprops, opt_with_no_revprops, opt_with_revprop,
     opt_auto_moves, 'x',},
    {{opt_with_revprop, N_("retrieve revision property ARG")},
     {'c', N_("the change made in revision ARG")}} },

  { "null-info", svn_cl__null_info, {0}, N_
    ("Display information about a local or remote item.\n"
     "usage: info [TARGET[@REV]...]\n"
     "\n"
     "  Print information about each TARGET (default: '.').\n"
     "  TARGET may be either a working-copy path or URL.  If specified, REV\n"
     "  determines in which revision the target is first looked up.\n"),
    {'r', 'R', opt_depth, opt_targets, opt_changelist}
  },

  { NULL, NULL, {0}, NULL, {0} }
};


/* Version compatibility check */
static svn_error_t *
check_lib_versions(void)
{
  static const svn_version_checklist_t checklist[] =
    {
      { "svn_subr",   svn_subr_version },
      { "svn_client", svn_client_version },
      { "svn_wc",     svn_wc_version },
      { "svn_ra",     svn_ra_version },
      { "svn_delta",  svn_delta_version },
      { NULL, NULL }
    };
  SVN_VERSION_DEFINE(my_version);

  return svn_ver_check_list2(&my_version, checklist, svn_ver_equal);
}


/* A flag to see if we've been cancelled by the client or not. */
static volatile sig_atomic_t cancelled = FALSE;

/* A signal handler to support cancellation. */
static void
signal_handler(int signum)
{
  apr_signal(signum, SIG_IGN);
  cancelled = TRUE;
}

/* Our cancellation callback. */
svn_error_t *
svn_cl__check_cancel(void *baton)
{
  if (cancelled)
    return svn_error_create(SVN_ERR_CANCELLED, NULL, _("Caught signal"));
  else
    return SVN_NO_ERROR;
}


/*** Main. ***/

/* Report and clear the error ERR, and return EXIT_FAILURE. */
#define EXIT_ERROR(err)                                                 \
  svn_cmdline_handle_exit_error(err, NULL, "svn: ")

/* A redefinition of the public SVN_INT_ERR macro, that suppresses the
 * error message if it is SVN_ERR_IO_PIPE_WRITE_ERROR. */
#undef SVN_INT_ERR
#define SVN_INT_ERR(expr)                                        \
  do {                                                           \
    svn_error_t *svn_err__temp = (expr);                         \
    if (svn_err__temp)                                           \
      return EXIT_ERROR(svn_err__temp);                          \
  } while (0)

static int
sub_main(int argc, const char *argv[], apr_pool_t *pool)
{
  svn_error_t *err;
  int opt_id;
  apr_getopt_t *os;
  svn_cl__opt_state_t opt_state = { 0, { 0 } };
  svn_client_ctx_t *ctx;
  apr_array_header_t *received_opts;
  int i;
  const svn_opt_subcommand_desc2_t *subcommand = NULL;
  svn_cl__cmd_baton_t command_baton;
  svn_auth_baton_t *ab;
  svn_config_t *cfg_config;
  svn_boolean_t descend = TRUE;
  svn_boolean_t use_notifier = TRUE;

  received_opts = apr_array_make(pool, SVN_OPT_MAX_OPTIONS, sizeof(int));

  /* Check library versions */
  SVN_INT_ERR(check_lib_versions());

#if defined(WIN32) || defined(__CYGWIN__)
  /* Set the working copy administrative directory name. */
  if (getenv("SVN_ASP_DOT_NET_HACK"))
    {
      SVN_INT_ERR(svn_wc_set_adm_dir("_svn", pool));
    }
#endif

  /* Initialize the RA library. */
  SVN_INT_ERR(svn_ra_initialize(pool));

  /* Begin processing arguments. */
  opt_state.start_revision.kind = svn_opt_revision_unspecified;
  opt_state.end_revision.kind = svn_opt_revision_unspecified;
  opt_state.revision_ranges =
    apr_array_make(pool, 0, sizeof(svn_opt_revision_range_t *));
  opt_state.depth = svn_depth_unknown;

  /* No args?  Show usage. */
  if (argc <= 1)
    {
      SVN_INT_ERR(svn_cl__help(NULL, NULL, pool));
      return EXIT_FAILURE;
    }

  /* Else, parse options. */
  SVN_INT_ERR(svn_cmdline__getopt_init(&os, argc, argv, pool));

  os->interleave = 1;
  while (1)
    {
      const char *opt_arg;
      const char *utf8_opt_arg;

      /* Parse the next option. */
      apr_status_t apr_err = apr_getopt_long(os, svn_cl__options, &opt_id,
                                             &opt_arg);
      if (APR_STATUS_IS_EOF(apr_err))
        break;
      else if (apr_err)
        {
          SVN_INT_ERR(svn_cl__help(NULL, NULL, pool));
          return EXIT_FAILURE;
        }

      /* Stash the option code in an array before parsing it. */
      APR_ARRAY_PUSH(received_opts, int) = opt_id;

      switch (opt_id) {
      case 'l':
        {
          err = svn_cstring_atoi(&opt_state.limit, opt_arg);
          if (err)
            {
              err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, err,
                                     _("Non-numeric limit argument given"));
              return EXIT_ERROR(err);
            }
          if (opt_state.limit <= 0)
            {
              err = svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                                    _("Argument to --limit must be positive"));
              return EXIT_ERROR(err);
            }
        }
        break;
      case 'c':
        {
          apr_array_header_t *change_revs =
            svn_cstring_split(opt_arg, ", \n\r\t\v", TRUE, pool);

          for (i = 0; i < change_revs->nelts; i++)
            {
              char *end;
              svn_revnum_t changeno, changeno_end;
              const char *change_str =
                APR_ARRAY_IDX(change_revs, i, const char *);
              const char *s = change_str;
              svn_boolean_t is_negative;

              /* Check for a leading minus to allow "-c -r42".
               * The is_negative flag is used to handle "-c -42" and "-c -r42".
               * The "-c r-42" case is handled by strtol() returning a
               * negative number. */
              is_negative = (*s == '-');
              if (is_negative)
                s++;

              /* Allow any number of 'r's to prefix a revision number. */
              while (*s == 'r')
                s++;
              changeno = changeno_end = strtol(s, &end, 10);
              if (end != s && *end == '-')
                {
                  if (changeno < 0 || is_negative)
                    {
                      err = svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR,
                                              NULL,
                                              _("Negative number in range (%s)"
                                                " not supported with -c"),
                                              change_str);
                      return EXIT_ERROR(err);
                    }
                  s = end + 1;
                  while (*s == 'r')
                    s++;
                  changeno_end = strtol(s, &end, 10);
                }
              if (end == change_str || *end != '\0')
                {
                  err = svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                          _("Non-numeric change argument (%s) "
                                            "given to -c"), change_str);
                  return EXIT_ERROR(err);
                }

              if (changeno == 0)
                {
                  err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                         _("There is no change 0"));
                  return EXIT_ERROR(err);
                }

              if (is_negative)
                changeno = -changeno;

              /* Figure out the range:
                    -c N  -> -r N-1:N
                    -c -N -> -r N:N-1
                    -c M-N -> -r M-1:N for M < N
                    -c M-N -> -r M:N-1 for M > N
                    -c -M-N -> error (too confusing/no valid use case)
              */
              if (changeno > 0)
                {
                  if (changeno <= changeno_end)
                    changeno--;
                  else
                    changeno_end--;
                }
              else
                {
                  changeno = -changeno;
                  changeno_end = changeno - 1;
                }

              opt_state.used_change_arg = TRUE;
              APR_ARRAY_PUSH(opt_state.revision_ranges,
                             svn_opt_revision_range_t *)
                = svn_opt__revision_range_from_revnums(changeno, changeno_end,
                                                       pool);
            }
        }
        break;
      case 'r':
        opt_state.used_revision_arg = TRUE;
        if (svn_opt_parse_revision_to_range(opt_state.revision_ranges,
                                            opt_arg, pool) != 0)
          {
            SVN_INT_ERR(svn_utf_cstring_to_utf8(&utf8_opt_arg, opt_arg, pool));
            err = svn_error_createf
                (SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                 _("Syntax error in revision argument '%s'"),
                 utf8_opt_arg);
            return EXIT_ERROR(err);
          }
        break;
      case 'v':
        opt_state.verbose = TRUE;
        break;
      case 'h':
      case '?':
        opt_state.help = TRUE;
        break;
      case 'q':
        opt_state.quiet = TRUE;
        break;
      case opt_targets:
        {
          svn_stringbuf_t *buffer, *buffer_utf8;

          /* We need to convert to UTF-8 now, even before we divide
             the targets into an array, because otherwise we wouldn't
             know what delimiter to use for svn_cstring_split().  */

          SVN_INT_ERR(svn_utf_cstring_to_utf8(&utf8_opt_arg, opt_arg, pool));
          SVN_INT_ERR(svn_stringbuf_from_file2(&buffer, utf8_opt_arg, pool));
          SVN_INT_ERR(svn_utf_stringbuf_to_utf8(&buffer_utf8, buffer, pool));
          opt_state.targets = svn_cstring_split(buffer_utf8->data, "\n\r",
                                                TRUE, pool);
        }
        break;
      case 'R':
        opt_state.depth = svn_depth_infinity;
        break;
      case 'N':
        descend = FALSE;
        break;
      case opt_depth:
        err = svn_utf_cstring_to_utf8(&utf8_opt_arg, opt_arg, pool);
        if (err)
          return EXIT_ERROR
            (svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, err,
                               _("Error converting depth "
                                 "from locale to UTF-8")));
        opt_state.depth = svn_depth_from_word(utf8_opt_arg);
        if (opt_state.depth == svn_depth_unknown
            || opt_state.depth == svn_depth_exclude)
          {
            return EXIT_ERROR
              (svn_error_createf(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                 _("'%s' is not a valid depth; try "
                                   "'empty', 'files', 'immediates', "
                                   "or 'infinity'"),
                                 utf8_opt_arg));
          }
        break;
      case opt_version:
        opt_state.version = TRUE;
        break;
      case opt_auth_username:
        SVN_INT_ERR(svn_utf_cstring_to_utf8(&opt_state.auth_username,
                                            opt_arg, pool));
        break;
      case opt_auth_password:
        SVN_INT_ERR(svn_utf_cstring_to_utf8(&opt_state.auth_password,
                                            opt_arg, pool));
        break;
      case opt_stop_on_copy:
        opt_state.stop_on_copy = TRUE;
        break;
      case opt_strict:
        opt_state.strict = TRUE;
        break;
      case opt_no_auth_cache:
        opt_state.no_auth_cache = TRUE;
        break;
      case opt_non_interactive:
        opt_state.non_interactive = TRUE;
        break;
      case opt_trust_server_cert:
        opt_state.trust_server_cert = TRUE;
        break;
      case 'x':
        SVN_INT_ERR(svn_utf_cstring_to_utf8(&opt_state.extensions,
                                            opt_arg, pool));
        break;
      case opt_config_dir:
        {
          const char *path_utf8;
          SVN_INT_ERR(svn_utf_cstring_to_utf8(&path_utf8, opt_arg, pool));
          opt_state.config_dir = svn_dirent_internal_style(path_utf8, pool);
        }
        break;
      case opt_config_options:
        if (!opt_state.config_options)
          opt_state.config_options =
                   apr_array_make(pool, 1,
                                  sizeof(svn_cmdline__config_argument_t*));

        SVN_INT_ERR(svn_utf_cstring_to_utf8(&opt_arg, opt_arg, pool));
        SVN_INT_ERR(svn_cmdline__parse_config_option(opt_state.config_options,
                                                     opt_arg, pool));
        break;
      case opt_with_all_revprops:
        /* If --with-all-revprops is specified along with one or more
         * --with-revprops options, --with-all-revprops takes precedence. */
        opt_state.all_revprops = TRUE;
        break;
      case opt_with_no_revprops:
        opt_state.no_revprops = TRUE;
        break;
      case opt_with_revprop:
        SVN_INT_ERR(svn_opt_parse_revprop(&opt_state.revprop_table,
                                          opt_arg, pool));
        break;
      case 'g':
        opt_state.use_merge_history = TRUE;
        break;
      case opt_auto_moves:
        opt_state.auto_moves = TRUE;
        break;
      default:
        /* Hmmm. Perhaps this would be a good place to squirrel away
           opts that commands like svn diff might need. Hmmm indeed. */
        break;
      }
    }

  /* ### This really belongs in libsvn_client.  The trouble is,
     there's no one place there to run it from, no
     svn_client_init().  We'd have to add it to all the public
     functions that a client might call.  It's unmaintainable to do
     initialization from within libsvn_client itself, but it seems
     burdensome to demand that all clients call svn_client_init()
     before calling any other libsvn_client function... On the other
     hand, the alternative is effectively to demand that they call
     svn_config_ensure() instead, so maybe we should have a generic
     init function anyway.  Thoughts?  */
  SVN_INT_ERR(svn_config_ensure(opt_state.config_dir, pool));

  /* If the user asked for help, then the rest of the arguments are
     the names of subcommands to get help on (if any), or else they're
     just typos/mistakes.  Whatever the case, the subcommand to
     actually run is svn_cl__help(). */
  if (opt_state.help)
    subcommand = svn_opt_get_canonical_subcommand2(svn_cl__cmd_table, "help");

  /* If we're not running the `help' subcommand, then look for a
     subcommand in the first argument. */
  if (subcommand == NULL)
    {
      if (os->ind >= os->argc)
        {
          if (opt_state.version)
            {
              /* Use the "help" subcommand to handle the "--version" option. */
              static const svn_opt_subcommand_desc2_t pseudo_cmd =
                { "--version", svn_cl__help, {0}, "",
                  {opt_version,    /* must accept its own option */
                   'q',            /* brief output */
                   'v',            /* verbose output */
                   opt_config_dir  /* all commands accept this */
                  } };

              subcommand = &pseudo_cmd;
            }
          else
            {
              svn_error_clear
                (svn_cmdline_fprintf(stderr, pool,
                                     _("Subcommand argument required\n")));
              SVN_INT_ERR(svn_cl__help(NULL, NULL, pool));
              return EXIT_FAILURE;
            }
        }
      else
        {
          const char *first_arg = os->argv[os->ind++];
          subcommand = svn_opt_get_canonical_subcommand2(svn_cl__cmd_table,
                                                         first_arg);
          if (subcommand == NULL)
            {
              const char *first_arg_utf8;
              SVN_INT_ERR(svn_utf_cstring_to_utf8(&first_arg_utf8,
                                                  first_arg, pool));
              svn_error_clear
                (svn_cmdline_fprintf(stderr, pool,
                                     _("Unknown subcommand: '%s'\n"),
                                     first_arg_utf8));
              SVN_INT_ERR(svn_cl__help(NULL, NULL, pool));
              return EXIT_FAILURE;
            }
        }
    }

  /* Check that the subcommand wasn't passed any inappropriate options. */
  for (i = 0; i < received_opts->nelts; i++)
    {
      opt_id = APR_ARRAY_IDX(received_opts, i, int);

      /* All commands implicitly accept --help, so just skip over this
         when we see it. Note that we don't want to include this option
         in their "accepted options" list because it would be awfully
         redundant to display it in every commands' help text. */
      if (opt_id == 'h' || opt_id == '?')
        continue;

      if (! svn_opt_subcommand_takes_option3(subcommand, opt_id,
                                             svn_cl__global_options))
        {
          const char *optstr;
          const apr_getopt_option_t *badopt =
            svn_opt_get_option_from_code2(opt_id, svn_cl__options,
                                          subcommand, pool);
          svn_opt_format_option(&optstr, badopt, FALSE, pool);
          if (subcommand->name[0] == '-')
            SVN_INT_ERR(svn_cl__help(NULL, NULL, pool));
          else
            svn_error_clear
              (svn_cmdline_fprintf
               (stderr, pool, _("Subcommand '%s' doesn't accept option '%s'\n"
                                "Type 'svn-bench help %s' for usage.\n"),
                subcommand->name, optstr, subcommand->name));
          return EXIT_FAILURE;
        }
    }

  /* Only merge and log support multiple revisions/revision ranges. */
  if (subcommand->cmd_func != svn_cl__null_log)
    {
      if (opt_state.revision_ranges->nelts > 1)
        {
          err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                 _("Multiple revision arguments "
                                   "encountered; can't specify -c twice, "
                                   "or both -c and -r"));
          return EXIT_ERROR(err);
        }
    }

  /* Disallow simultaneous use of both --with-all-revprops and
     --with-no-revprops.  */
  if (opt_state.all_revprops && opt_state.no_revprops)
    {
      err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                             _("--with-all-revprops and --with-no-revprops "
                               "are mutually exclusive"));
      return EXIT_ERROR(err);
    }

  /* Disallow simultaneous use of both --with-revprop and
     --with-no-revprops.  */
  if (opt_state.revprop_table && opt_state.no_revprops)
    {
      err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                             _("--with-revprop and --with-no-revprops "
                               "are mutually exclusive"));
      return EXIT_ERROR(err);
    }

  /* --trust-server-cert can only be used with --non-interactive */
  if (opt_state.trust_server_cert && !opt_state.non_interactive)
    {
      err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                             _("--trust-server-cert requires "
                               "--non-interactive"));
      return EXIT_ERROR(err);
    }

  /* Ensure that 'revision_ranges' has at least one item, and make
     'start_revision' and 'end_revision' match that item. */
  if (opt_state.revision_ranges->nelts == 0)
    {
      svn_opt_revision_range_t *range = apr_palloc(pool, sizeof(*range));
      range->start.kind = svn_opt_revision_unspecified;
      range->end.kind = svn_opt_revision_unspecified;
      APR_ARRAY_PUSH(opt_state.revision_ranges,
                     svn_opt_revision_range_t *) = range;
    }
  opt_state.start_revision = APR_ARRAY_IDX(opt_state.revision_ranges, 0,
                                           svn_opt_revision_range_t *)->start;
  opt_state.end_revision = APR_ARRAY_IDX(opt_state.revision_ranges, 0,
                                         svn_opt_revision_range_t *)->end;

  /* Create a client context object. */
  command_baton.opt_state = &opt_state;
  SVN_INT_ERR(svn_client_create_context2(&ctx, NULL, pool));
  command_baton.ctx = ctx;

  /* Only a few commands can accept a revision range; the rest can take at
     most one revision number. */
  if (subcommand->cmd_func != svn_cl__null_log)
    {
      if (opt_state.end_revision.kind != svn_opt_revision_unspecified)
        {
          err = svn_error_create(SVN_ERR_CLIENT_REVISION_RANGE, NULL, NULL);
          return EXIT_ERROR(err);
        }
    }

  /* -N has a different meaning depending on the command */
  if (!descend)
    opt_state.depth = svn_depth_files;

  err = svn_config_get_config(&(ctx->config),
                              opt_state.config_dir, pool);
  if (err)
    {
      /* Fallback to default config if the config directory isn't readable
         or is not a directory. */
      if (APR_STATUS_IS_EACCES(err->apr_err)
          || SVN__APR_STATUS_IS_ENOTDIR(err->apr_err))
        {
          svn_handle_warning2(stderr, err, "svn: ");
          svn_error_clear(err);
        }
      else
        return EXIT_ERROR(err);
    }

  cfg_config = apr_hash_get(ctx->config, SVN_CONFIG_CATEGORY_CONFIG,
                            APR_HASH_KEY_STRING);

  /* Update the options in the config */
  if (opt_state.config_options)
    {
      svn_error_clear(
          svn_cmdline__apply_config_options(ctx->config,
                                            opt_state.config_options,
                                            "svn: ", "--config-option"));
    }

  /* Set up the notifier.

     In general, we use it any time we aren't in --quiet mode.  'svn
     status' is unique, though, in that we don't want it in --quiet mode
     unless we're also in --verbose mode.  When in --xml mode,
     though, we never want it.  */
  if (opt_state.quiet)
    use_notifier = FALSE;
  if (use_notifier)
    {
      SVN_INT_ERR(svn_cl__get_notifier(&ctx->notify_func2, &ctx->notify_baton2,
                                       pool));
    }

  /* Set up our cancellation support. */
  ctx->cancel_func = svn_cl__check_cancel;
  apr_signal(SIGINT, signal_handler);
#ifdef SIGBREAK
  /* SIGBREAK is a Win32 specific signal generated by ctrl-break. */
  apr_signal(SIGBREAK, signal_handler);
#endif
#ifdef SIGHUP
  apr_signal(SIGHUP, signal_handler);
#endif
#ifdef SIGTERM
  apr_signal(SIGTERM, signal_handler);
#endif

#ifdef SIGPIPE
  /* Disable SIGPIPE generation for the platforms that have it. */
  apr_signal(SIGPIPE, SIG_IGN);
#endif

#ifdef SIGXFSZ
  /* Disable SIGXFSZ generation for the platforms that have it, otherwise
   * working with large files when compiled against an APR that doesn't have
   * large file support will crash the program, which is uncool. */
  apr_signal(SIGXFSZ, SIG_IGN);
#endif

  /* Set up Authentication stuff. */
  SVN_INT_ERR(svn_cmdline_create_auth_baton(&ab,
                                            opt_state.non_interactive,
                                            opt_state.auth_username,
                                            opt_state.auth_password,
                                            opt_state.config_dir,
                                            opt_state.no_auth_cache,
                                            opt_state.trust_server_cert,
                                            cfg_config,
                                            ctx->cancel_func,
                                            ctx->cancel_baton,
                                            pool));

  ctx->auth_baton = ab;

  /* The new svn behavior is to postpone everything until after the operation
     completed */
  ctx->conflict_func = NULL;
  ctx->conflict_baton = NULL;
  ctx->conflict_func2 = NULL;
  ctx->conflict_baton2 = NULL;

  /* And now we finally run the subcommand. */
  err = (*subcommand->cmd_func)(os, &command_baton, pool);
  if (err)
    {
      /* For argument-related problems, suggest using the 'help'
         subcommand. */
      if (err->apr_err == SVN_ERR_CL_INSUFFICIENT_ARGS
          || err->apr_err == SVN_ERR_CL_ARG_PARSING_ERROR)
        {
          err = svn_error_quick_wrap(
                  err, apr_psprintf(pool,
                                    _("Try 'svn-bench help %s' for more information"),
                                    subcommand->name));
        }
      if (err->apr_err == SVN_ERR_WC_UPGRADE_REQUIRED)
        {
          err = svn_error_quick_wrap(err,
                                     _("Please see the 'svn upgrade' command"));
        }

      /* Tell the user about 'svn cleanup' if any error on the stack
         was about locked working copies. */
      if (svn_error_find_cause(err, SVN_ERR_WC_LOCKED))
        {
          err = svn_error_quick_wrap(
                  err, _("Run 'svn cleanup' to remove locks "
                         "(type 'svn help cleanup' for details)"));
        }

      return EXIT_ERROR(err);
    }
  else
    {
      /* Ensure that stdout is flushed, so the user will see any write errors.
         This makes sure that output is not silently lost. */
      SVN_INT_ERR(svn_cmdline_fflush(stdout));

      return EXIT_SUCCESS;
    }
}

int
main(int argc, const char *argv[])
{
  apr_pool_t *pool;
  int exit_code;

  /* Initialize the app. */
  if (svn_cmdline_init("svn", stderr) != EXIT_SUCCESS)
    return EXIT_FAILURE;

  /* Create our top-level pool.  Use a separate mutexless allocator,
   * given this application is single threaded.
   */
  pool = apr_allocator_owner_get(svn_pool_create_allocator(FALSE));

  exit_code = sub_main(argc, argv, pool);

  svn_pool_destroy(pool);
  return exit_code;
}
