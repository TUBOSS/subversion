/*
 * log.c :  entry point for log RA functions for ra_serf
 *
 * ====================================================================
 * Copyright (c) 2006 CollabNet.  All rights reserved.
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



#include <apr_uri.h>

#include <expat.h>

#include <serf.h>

#include "svn_pools.h"
#include "svn_ra.h"
#include "svn_dav.h"
#include "svn_xml.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_config.h"
#include "svn_delta.h"
#include "svn_version.h"
#include "svn_path.h"
#include "svn_private_config.h"

#include "ra_serf.h"


/*
 * This enum represents the current state of our XML parsing for a REPORT.
 */
typedef enum {
  REPORT,
  ITEM,
  VERSION,
  CREATOR,
  DATE,
  COMMENT,
} log_state_e;

typedef struct {
  /* The currently collected value as we build it up */
  const char *tmp;
  apr_size_t tmp_len;

  /* Hashtable of paths */
  /* TODO implement changed-paths support */
  apr_hash_t *paths;

  /* Other log fields */
  svn_revnum_t version;
  const char *creator;
  const char *date;
  const char *comment;
} log_info_t;

typedef struct log_state_list_t {
  /* The current state that we are in now. */
  log_state_e state;

  /* Information */
  log_info_t *info;

  /* The previous state we were in. */
  struct log_state_list_t *prev;
} log_state_list_t;

typedef struct {
  apr_pool_t *pool;

  /* parameters set by our caller */
  int limit;
  int count;
  svn_boolean_t changed_paths;

  /* Current namespace list */
  svn_ra_serf__ns_t *ns_list;

  /* Current state we're in */
  log_state_list_t *state;
  log_state_list_t *free_state;

  /* Return error code; if we exceed count, this may be set */
  svn_error_t *error;

  /* are we done? */
  svn_boolean_t done;

  /* log receiver function and baton */
  svn_log_message_receiver_t receiver;
  void *receiver_baton;
} log_context_t;


static void
push_state(log_context_t *log_ctx, log_state_e state)
{
  log_state_list_t *new_state;

  if (!log_ctx->free_state)
    {
      new_state = apr_palloc(log_ctx->pool, sizeof(*log_ctx->state));
      new_state->info = NULL;
    }
  else
    {
      new_state = log_ctx->free_state;
      log_ctx->free_state = log_ctx->free_state->prev;
    }
  new_state->state = state;

  if (state == REPORT)
    {
      /* do nothing for now */
    }
  else if (state == ITEM)
    {
      if (!new_state->info)
        {
          new_state->info = apr_palloc(log_ctx->pool, sizeof(*new_state->info));
        }

      new_state->info->paths = NULL;
      new_state->info->version = SVN_INVALID_REVNUM;
      new_state->info->creator = NULL;
      new_state->info->date = NULL;
      new_state->info->comment = NULL;
      new_state->info->tmp = NULL;
      new_state->info->tmp_len = 0;
    }
  /* if we have state info from our parent, reuse it. */
  else if (log_ctx->state && log_ctx->state->info)
    {
      new_state->info = log_ctx->state->info;
    }
  else
    {
      abort();
    }

  /* Add it to the state chain. */
  new_state->prev = log_ctx->state;
  log_ctx->state = new_state;
}

static void pop_state(log_context_t *log_ctx)
{
  log_state_list_t *free_state;
  free_state = log_ctx->state;
  /* advance the current state */
  log_ctx->state = log_ctx->state->prev;
  free_state->prev = log_ctx->free_state;
  log_ctx->free_state = free_state;
  /* It's okay to reuse our info. */
  /* ctx->free_state->info = NULL; */
}

static void XMLCALL
start_log(void *userData, const char *raw_name, const char **attrs)
{
  log_context_t *log_ctx = userData;
  svn_ra_serf__dav_props_t name;

  svn_ra_serf__define_ns(&log_ctx->ns_list, attrs, log_ctx->pool);

  name = svn_ra_serf__expand_ns(log_ctx->ns_list, raw_name);

  if (!log_ctx->state && strcmp(name.name, "log-report") == 0)
    {
      push_state(log_ctx, REPORT);
    }
  else if (log_ctx->state &&
           log_ctx->state->state == REPORT &&
           strcmp(name.name, "log-item") == 0)
    {
      log_ctx->count++;
      if (log_ctx->limit && log_ctx->count > log_ctx->limit)
        {
          abort();
        }

      push_state(log_ctx, ITEM);
    }
  else if (log_ctx->state &&
           log_ctx->state->state == ITEM)
    {
      if (strcmp(name.name, "version-name") == 0)
        {
          push_state(log_ctx, VERSION);
        }
      else if (strcmp(name.name, "creator-displayname") == 0)
        {
          push_state(log_ctx, CREATOR);
        }
      else if (strcmp(name.name, "date") == 0)
        {
          push_state(log_ctx, DATE);
        }
      else if (strcmp(name.name, "comment") == 0)
        {
          push_state(log_ctx, COMMENT);
        }
    }
}

static void XMLCALL
end_log(void *userData, const char *raw_name)
{
  log_context_t *log_ctx = userData;
  svn_ra_serf__dav_props_t name;
  log_state_list_t *cur_state;

  if (!log_ctx->state)
    {
      return;
    }

  cur_state = log_ctx->state;

  name = svn_ra_serf__expand_ns(log_ctx->ns_list, raw_name);

  if (cur_state->state == REPORT &&
      strcmp(name.name, "log-report") == 0)
    {
      pop_state(log_ctx);
    }
  else if (cur_state->state == ITEM &&
           strcmp(name.name, "log-item") == 0)
    {
      /* Give the info to the reporter */
      log_ctx->receiver(log_ctx->receiver_baton,
                        cur_state->info->paths,
                        cur_state->info->version,
                        cur_state->info->creator,
                        cur_state->info->date,
                        cur_state->info->comment,
                        log_ctx->pool);

      pop_state(log_ctx);
    }
  else if (cur_state->state == VERSION &&
           strcmp(name.name, "version-name") == 0)
    {
      cur_state->info->version = SVN_STR_TO_REV(cur_state->info->tmp);
      cur_state->info->tmp = NULL;
      pop_state(log_ctx);
    }
  else if (cur_state->state == CREATOR &&
           strcmp(name.name, "creator-displayname") == 0)
    {
      cur_state->info->creator = cur_state->info->tmp;
      cur_state->info->tmp = NULL;
      pop_state(log_ctx);
    }
  else if (cur_state->state == DATE &&
           strcmp(name.name, "date") == 0)
    {
      cur_state->info->date = cur_state->info->tmp;
      cur_state->info->tmp = NULL;
      pop_state(log_ctx);
    }
  else if (cur_state->state == COMMENT &&
           strcmp(name.name, "comment") == 0)
    {
      cur_state->info->comment = cur_state->info->tmp;
      cur_state->info->tmp = NULL;
      pop_state(log_ctx);
    }
}

static void XMLCALL
cdata_log(void *userData, const char *data, int len)
{
  log_context_t *log_ctx = userData;

  if (!log_ctx->state)
    {
      return;
    }

  switch (log_ctx->state->state)
    {
      case VERSION:
      case CREATOR:
      case DATE:
      case COMMENT:
        svn_ra_serf__expand_string(&log_ctx->state->info->tmp,
                                   &log_ctx->state->info->tmp_len,
                                   data, len, log_ctx->pool);
        break;
      default:
        break;
    }
}

svn_error_t *
svn_ra_serf__get_log(svn_ra_session_t *ra_session,
                     const apr_array_header_t *paths,
                     svn_revnum_t start,
                     svn_revnum_t end,
                     int limit,
                     svn_boolean_t discover_changed_paths,
                     svn_boolean_t strict_node_history,
                     svn_log_message_receiver_t receiver,
                     void *receiver_baton,
                     apr_pool_t *pool)
{
  log_context_t *log_ctx;
  svn_ra_serf__session_t *session = ra_session->priv;
  svn_ra_serf__handler_t *handler;
  svn_ra_serf__xml_parser_t *parser_ctx;
  serf_bucket_t *buckets, *tmp;
  apr_hash_t *props;
  svn_revnum_t *peg_rev;
  const char *vcc_url, *relative_url, *baseline_url, *basecoll_url, *req_url;

  log_ctx = apr_pcalloc(pool, sizeof(*log_ctx));
  log_ctx->pool = pool;
  log_ctx->receiver = receiver;
  log_ctx->receiver_baton = receiver_baton;
  log_ctx->limit = limit;
  log_ctx->changed_paths = discover_changed_paths;
  log_ctx->error = SVN_NO_ERROR;
  log_ctx->done = FALSE;

  buckets = serf_bucket_aggregate_create(session->bkt_alloc);

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN("<S:log-report xmlns:S=\"",
                                      sizeof("<S:log-report xmlns:S=\"")-1,
                                      session->bkt_alloc);
  serf_bucket_aggregate_append(buckets, tmp);

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN(SVN_XML_NAMESPACE,
                                      sizeof(SVN_XML_NAMESPACE)-1,
                                      session->bkt_alloc);
  serf_bucket_aggregate_append(buckets, tmp);

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN("\">",
                                      sizeof("\">")-1,
                                      session->bkt_alloc);
  serf_bucket_aggregate_append(buckets, tmp);

  add_tag_buckets(buckets,
                  "S:start-revision", apr_ltoa(pool, start),
                  session->bkt_alloc);
  add_tag_buckets(buckets,
                  "S:end-revision", apr_ltoa(pool, end),
                  session->bkt_alloc);

  if (limit)
    {
      add_tag_buckets(buckets,
                      "S:limit", apr_ltoa(pool, limit),
                      session->bkt_alloc);
    }

  if (discover_changed_paths)
    {
      add_tag_buckets(buckets,
                      "S:discover-changed-paths", NULL,
                      session->bkt_alloc);
    }

  if (strict_node_history)
    {
      add_tag_buckets(buckets,
                      "S:strict-node-history", NULL,
                      session->bkt_alloc);
    }

  if (paths)
    {
      int i;
      for (i = 0; i < paths->nelts; i++)
        {
          add_tag_buckets(buckets,
                          "S:path", ((const char**)paths->elts)[i],
                          session->bkt_alloc);
        }
    }

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN("</S:log-report>",
                                      sizeof("</S:log-report>")-1,
                                      session->bkt_alloc);
  serf_bucket_aggregate_append(buckets, tmp);

  props = apr_hash_make(pool);

  SVN_ERR(svn_ra_serf__discover_root(&vcc_url, &relative_url,
                                     session, session->conns[0],
                                     session->repos_url.path, pool));

  /* At this point, we may have a deleted file.  So, we'll match ra_dav's
   * behavior and use the larger of start or end as our 'peg' rev.
   */
  peg_rev = (start > end) ? start : end;

  SVN_ERR(svn_ra_serf__retrieve_props(props, session, session->conns[0],
                                      vcc_url, peg_rev, "0",
                                      checked_in_props, pool));

  baseline_url = svn_ra_serf__get_ver_prop(props, vcc_url, peg_rev,
                                           "DAV:", "href");

  if (!baseline_url)
    {
      abort();
    }

  SVN_ERR(svn_ra_serf__retrieve_props(props, session, session->conns[0],
                                      baseline_url, peg_rev, "0",
                                      baseline_props, pool));

  basecoll_url = svn_ra_serf__get_ver_prop(props, baseline_url, peg_rev,
                                           "DAV:", "baseline-collection");

  if (!basecoll_url)
    {
      abort();
    }

  req_url = svn_path_url_add_component(basecoll_url, relative_url, pool);

  handler = apr_pcalloc(pool, sizeof(*handler));

  handler->method = "REPORT";
  handler->path = req_url;
  handler->body_buckets = buckets;
  handler->body_type = "text/xml";
  handler->conn = session->conns[0];
  handler->session = session;

  parser_ctx = apr_pcalloc(pool, sizeof(*parser_ctx));

  parser_ctx->user_data = log_ctx;
  parser_ctx->start = start_log;
  parser_ctx->end = end_log;
  parser_ctx->cdata = cdata_log;
  parser_ctx->done = &log_ctx->done;

  handler->response_handler = svn_ra_serf__handle_xml_parser;
  handler->response_baton = parser_ctx;

  svn_ra_serf__request_create(handler);

  SVN_ERR(svn_ra_serf__context_run_wait(&log_ctx->done, session, pool));

  return log_ctx->error;
}
