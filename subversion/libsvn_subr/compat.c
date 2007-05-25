/*
 * compat.c :  Wrappers and callbacks for compatibility.
 *
 * ====================================================================
 * Copyright (c) 2005 CollabNet.  All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at http://subversion.tigris.org/license-1.html.
 * If newer versions of this license are posted there, you may use a
 * newer version instead, at your option.
 *
 * This software consists of voluntary contributions made by many
 * individuals.  For exact contribution history, see the revision
 * history and logs, available at http://subversion.tigris.org/.
 * ====================================================================
 */

#include <apr_pools.h>
#include <apr_strings.h>

#include "svn_types.h"
#include "svn_error.h"


/* Baton for use with svn_compat_wrap_commit_callback */
struct commit_wrapper_baton {
  void *baton;
  svn_commit_callback_t callback;
};

/* This implements svn_commit_callback2_t. */
static svn_error_t *
commit_wrapper_callback(const svn_commit_info_t *commit_info,
                        void *baton, apr_pool_t *pool)
{
  struct commit_wrapper_baton *cwb = baton;

  if (cwb->callback)
    return cwb->callback(commit_info->revision,
                         commit_info->date,
                         commit_info->author,
                         cwb->baton);

  return SVN_NO_ERROR;
}

void
svn_compat_wrap_commit_callback(svn_commit_callback2_t *callback2,
                                void **callback2_baton,
                                svn_commit_callback_t callback,
                                void *callback_baton,
                                apr_pool_t *pool)
{
  struct commit_wrapper_baton *cwb = apr_palloc(pool, sizeof(*cwb));

  /* Set the user provided old format callback in the baton */
  cwb->baton = callback_baton;
  cwb->callback = callback;

  *callback2_baton = cwb;
  *callback2 = commit_wrapper_callback;
}


/* Baton for use with svn_compat_wrap_log_receiver */
struct log_wrapper_baton {
  void *baton;
  svn_log_message_receiver_t receiver;
};

/* This implements svn_log_message_receiver2_t. */
static svn_error_t *
log_wrapper_callback(void *baton,
                     svn_log_entry_t *log_entry,
                     apr_pool_t *pool)
{
  struct log_wrapper_baton *lwb = baton;

  if (lwb->receiver)
    return lwb->receiver(lwb->baton,
                         log_entry->changed_paths,
                         log_entry->revision,
                         log_entry->author,
                         log_entry->date,
                         log_entry->message,
                         pool);

  return SVN_NO_ERROR;
}

void
svn_compat_wrap_log_receiver(svn_log_message_receiver2_t *receiver2,
                             void **receiver2_baton,
                             svn_log_message_receiver_t receiver,
                             void *receiver_baton,
                             apr_pool_t *pool)
{
  struct log_wrapper_baton *lwb = apr_palloc(pool, sizeof(*lwb));

  /* Set the user provided old format callback in the baton. */
  lwb->baton = receiver_baton;
  lwb->receiver = receiver;

  *receiver2_baton = lwb;
  *receiver2 = log_wrapper_callback;
}
