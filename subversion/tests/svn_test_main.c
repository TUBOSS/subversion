/*
 * tests-main.c:  shared main() & friends for SVN test-suite programs
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



#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <apr_pools.h>
#include <apr_general.h>

#include "svn_cmdline.h"
#include "svn_opt.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_test.h"
#include "svn_io.h"
#include "svn_path.h"
#include "svn_ctype.h"
#include "svn_private_config.h"


/* Some Subversion test programs may want to parse options in the
   argument list, so we remember it here. */
int test_argc;
const char **test_argv;


/* Test option: Print more output */
static svn_boolean_t verbose_mode = FALSE;

/* Test option: Trap SVN_ERR_ASSERT failures in the code under test. Default
 * is false so the test can easily be run in a debugger with the debugger
 * catching the assertion failure. Test suites should enable this in order
 * to be able to continue with other sub-tests and report the results even
 * when a test hits an assertion failure. */
static svn_boolean_t trap_assertion_failures = FALSE;

/* Test option: Print only unexpected results */
static svn_boolean_t quiet_mode = FALSE;

/* Test option: Remove test directories after success */
static svn_boolean_t cleanup_mode = FALSE;

/* Option parsing enums and structures */
enum {
  cleanup_opt = SVN_OPT_FIRST_LONGOPT_ID,
  fstype_opt,
  list_opt,
  verbose_opt,
  trap_assert_opt,
  quiet_opt,
  config_opt,
  server_minor_version_opt
};

static const apr_getopt_option_t cl_options[] =
{
  {"cleanup",       cleanup_opt, 0,
                    N_("remove test directories after success")},
  {"config-file",   config_opt, 1,
                    N_("specify test config file ARG")},
  {"fs-type",       fstype_opt, 1,
                    N_("specify a filesystem backend type ARG")},
  {"list",          list_opt, 0,
                    N_("lists all the tests with their short description")},
  {"verbose",       verbose_opt, 0,
                    N_("print extra information")},
  {"server-minor-version", server_minor_version_opt, 1,
                    N_("set the minor version for the server ('3', '4',\n"
                       "'5', or '6')")},
  {"trap-assertion-failures", trap_assert_opt, 0,
                    N_("catch and report SVN_ERR_ASSERT failures")},
  {"quiet",         quiet_opt, 0,
                    N_("print only unexpected results")},
  {0,               0, 0, 0}
};


/* ================================================================= */
/* Stuff for cleanup processing */

/* When non-zero, don't remove test directories */
static svn_boolean_t skip_cleanup = FALSE;

/* All cleanup actions are registered as cleanups on this pool. */
static apr_pool_t *cleanup_pool = NULL;

static apr_status_t
cleanup_rmtree(void *data)
{
  if (!skip_cleanup)
    {
      apr_pool_t *pool = svn_pool_create(NULL);
      const char *path = data;

      /* Ignore errors here. */
      svn_error_t *err = svn_io_remove_dir2(path, FALSE, NULL, NULL, pool);
      svn_error_clear(err);
      if (verbose_mode)
        {
          if (err)
            printf("FAILED CLEANUP: %s\n", path);
          else
            printf("CLEANUP: %s\n", path);
        }
      svn_pool_destroy(pool);
    }
  return APR_SUCCESS;
}


void
svn_test_add_dir_cleanup(const char *path)
{
  if (cleanup_mode)
    {
      const char *abspath;
      svn_error_t *err = svn_path_get_absolute(&abspath, path, cleanup_pool);
      svn_error_clear(err);
      if (!err)
        apr_pool_cleanup_register(cleanup_pool, abspath, cleanup_rmtree,
                                  apr_pool_cleanup_null);
      else if (verbose_mode)
        printf("FAILED ABSPATH: %s\n", path);
    }
}


/* ================================================================= */
/* Quite a few tests use random numbers. */

apr_uint32_t
svn_test_rand(apr_uint32_t *seed)
{
  *seed = (*seed * 1103515245UL + 12345UL) & 0xffffffffUL;
  return *seed;
}


/* ================================================================= */


/* Determine the array size of test_funcs[], the inelegant way.  :)  */
static int
get_array_size(void)
{
  int i;

  for (i = 1; test_funcs[i].func2 || test_funcs[i].func_opts; i++)
    {
    }

  return (i - 1);
}



/* Execute a test number TEST_NUM.  Pretty-print test name and dots
   according to our test-suite spec, and return the result code. */
static svn_boolean_t
do_test_num(const char *progname,
            int test_num,
            svn_boolean_t msg_only,
            svn_test_opts_t *opts,
            apr_pool_t *pool)
{
  svn_boolean_t skip, xfail, wimp;
  svn_error_t *err = NULL;
  svn_boolean_t test_failed;
  const char *msg = NULL;  /* the message this individual test prints out */
  const struct svn_test_descriptor_t *desc;
  const int array_size = get_array_size();

  /* Check our array bounds! */
  if (test_num < 0)
    test_num += array_size + 1;
  if ((test_num > array_size) || (test_num <= 0))
    {
      printf("FAIL: %s: THERE IS NO TEST NUMBER %2d\n", progname, test_num);
      skip_cleanup = TRUE;
      return TRUE;  /* BAIL, this test number doesn't exist. */
    }

  desc = &test_funcs[test_num];
  skip = desc->mode == svn_test_skip;
  xfail = desc->mode == svn_test_xfail;
  wimp = xfail && desc->wip;

  /* Do test */
  msg = desc->msg;
  if (msg_only || skip)
    ; /* pass */
  else if (desc->func2)
    err = (*desc->func2)(pool);
  else
    err = (*desc->func_opts)(opts, pool);

  if (err && err->apr_err == SVN_ERR_TEST_SKIPPED)
    {
      svn_error_clear(err);
      err = SVN_NO_ERROR;
      skip = TRUE;
    }

  /* Failure means unexpected results -- FAIL or XPASS. */
  test_failed = (!wimp && ((err != SVN_NO_ERROR) != (xfail != 0)));

  /* If we got an error, print it out.  */
  if (err)
    {
      svn_handle_error2(err, stdout, FALSE, "svn_tests: ");
      svn_error_clear(err);
    }

  if (msg_only)
    {
      printf(" %2d     %-5s  %s%s%s%s\n",
             test_num,
             (xfail ? "XFAIL" : (skip ? "SKIP" : "")),
             msg ? msg : "(test did not provide name)",
             (wimp && verbose_mode) ? " [[" : "",
             (wimp && verbose_mode) ? desc->wip : "",
             (wimp && verbose_mode) ? "]]" : "");
    }
  else if ((! quiet_mode) || test_failed)
    {
      printf("%s %s %d: %s%s%s%s\n",
             (err
              ? (xfail ? "XFAIL:" : "FAIL: ")
              : (xfail ? "XPASS:" : (skip ? "SKIP: " : "PASS: "))),
             progname,
             test_num,
             msg ? msg : "(test did not provide name)",
             wimp ? " [[WIMP: " : "",
             wimp ? desc->wip : "",
             wimp ? "]]" : "");
    }

  if (msg)
    {
      int len = strlen(msg);
      if (len > 50)
        printf("WARNING: Test docstring exceeds 50 characters\n");
      if (msg[len - 1] == '.')
        printf("WARNING: Test docstring ends in a period (.)\n");
      if (svn_ctype_isupper(msg[0]))
        printf("WARNING: Test docstring is capitalized\n");
    }
  if (desc->msg == NULL)
    printf("WARNING: New-style test descriptor is missing a docstring.\n");

  skip_cleanup = test_failed;

  return test_failed;
}


/* Standard svn test program */
int
main(int argc, const char *argv[])
{
  const char *prog_name;
  int i;
  svn_boolean_t got_error = FALSE;
  apr_pool_t *pool, *test_pool;
  svn_boolean_t ran_a_test = FALSE;
  svn_boolean_t list_mode = FALSE;
  int opt_id;
  apr_status_t apr_err;
  apr_getopt_t *os;
  svn_error_t *err;
  char errmsg[200];
  /* How many tests are there? */
  int array_size = get_array_size();

  svn_test_opts_t opts = { NULL };

  opts.fs_type = DEFAULT_FS_TYPE;

  /* Initialize APR (Apache pools) */
  if (apr_initialize() != APR_SUCCESS)
    {
      printf("apr_initialize() failed.\n");
      exit(1);
    }

  /* set up the global pool */
  pool = svn_pool_create(NULL);

  /* Remember the command line */
  test_argc = argc;
  test_argv = argv;

  err = svn_cmdline__getopt_init(&os, argc, argv, pool);

  /* Strip off any leading path components from the program name.  */
  prog_name = strrchr(argv[0], '/');
  if (prog_name)
    prog_name++;
  else
    {
      /* Just check if this is that weird platform that uses \ instead
         of / for the path separator. */
      prog_name = strrchr(argv[0], '\\');
      if (prog_name)
        prog_name++;
      else
        prog_name = argv[0];
    }

  if (err)
    return svn_cmdline_handle_exit_error(err, pool, prog_name);
  while (1)
    {
      const char *opt_arg;

      /* Parse the next option. */
      apr_err = apr_getopt_long(os, cl_options, &opt_id, &opt_arg);
      if (APR_STATUS_IS_EOF(apr_err))
        break;
      else if (apr_err && (apr_err != APR_BADCH))
        {
          /* Ignore invalid option error to allow passing arbitary options */
          fprintf(stderr, "apr_getopt_long failed : [%d] %s\n",
                  apr_err, apr_strerror(apr_err, errmsg, sizeof(errmsg)));
          exit(1);
        }

      switch (opt_id) {
        case cleanup_opt:
          cleanup_mode = TRUE;
          break;
        case config_opt:
          opts.config_file = apr_pstrdup(pool, opt_arg);
          break;
        case fstype_opt:
          opts.fs_type = apr_pstrdup(pool, opt_arg);
          break;
        case list_opt:
          list_mode = TRUE;
          break;
        case verbose_opt:
          verbose_mode = TRUE;
          break;
        case trap_assert_opt:
          trap_assertion_failures = TRUE;
          break;
        case quiet_opt:
          quiet_mode = TRUE;
          break;
        case server_minor_version_opt:
          {
            char *end;
            opts.server_minor_version = strtol(opt_arg, &end, 10);
            if (end == opt_arg || *end != '\0')
              {
                fprintf(stderr, "FAIL: Non-numeric minor version given\n");
                exit(1);
              }
            if ((opts.server_minor_version < 3)
                || (opts.server_minor_version > 6))
              {
                fprintf(stderr, "FAIL: Invalid minor version given\n");
                exit(1);
              }
          }
      }
    }

  /* You can't be both quiet and verbose. */
  if (quiet_mode && verbose_mode)
    {
      fprintf(stderr, "FAIL: --verbose and --quiet are mutually exclusive\n");
      exit(1);
    }

  /* Create an iteration pool for the tests */
  cleanup_pool = svn_pool_create(pool);
  test_pool = svn_pool_create(pool);

  if (trap_assertion_failures)
    svn_error_set_malfunction_handler(svn_error_raise_on_malfunction);

  if (argc >= 2)  /* notice command-line arguments */
    {
      if (! strcmp(argv[1], "list") || list_mode)
        {
          ran_a_test = TRUE;

          /* run all tests with MSG_ONLY set to TRUE */

          printf("Test #  Mode   Test Description\n"
                 "------  -----  ----------------\n");
          for (i = 1; i <= array_size; i++)
            {
              if (do_test_num(prog_name, i, TRUE, &opts, test_pool))
                got_error = TRUE;

              /* Clear the per-function pool */
              svn_pool_clear(test_pool);
              svn_pool_clear(cleanup_pool);
            }
        }
      else
        {
          for (i = 1; i < argc; i++)
            {
              if (svn_ctype_isdigit(argv[i][0]) || argv[i][0] == '-')
                {
                  int test_num = atoi(argv[i]);
                  if (test_num == 0)
                    /* A --option argument, most likely. */
                    continue;

                  ran_a_test = TRUE;
                  if (do_test_num(prog_name, test_num, FALSE, &opts, test_pool))
                    got_error = TRUE;

                  /* Clear the per-function pool */
                  svn_pool_clear(test_pool);
                  svn_pool_clear(cleanup_pool);
                }
            }
        }
    }

  if (! ran_a_test)
    {
      /* just run all tests */
      for (i = 1; i <= array_size; i++)
        {
          if (do_test_num(prog_name, i, FALSE, &opts, test_pool))
            got_error = TRUE;

          /* Clear the per-function pool */
          svn_pool_clear(test_pool);
          svn_pool_clear(cleanup_pool);
        }
    }

  /* Clean up APR */
  svn_pool_destroy(pool);      /* takes test_pool with it */
  apr_terminate();

  exit(got_error);
  return got_error;
}
