/*
 * merge.c: handle the MERGE response processing
 *
 * ====================================================================
 * Copyright (c) 2000-2001 CollabNet.  All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at http://subversion.tigris.org/license-1.html.
 * If newer versions of this license are posted there, you may use a
 * newer version instead, at your option.
 * ====================================================================
 */

#include <apr_pools.h>
#include <apr_buckets.h>
#include <apr_xml.h>

#include <httpd.h>
#include <util_filter.h>

#include "svn_fs.h"

#include "dav_svn.h"


/* #################################################################

   These functions are currently *VERY* SVN specific.

   * we don't check prop_elem for what the client requested
   * we presume a baseline was checked out into the activity, and is
     part of the MERGE
   * we presume that all "changed" files/dirs were checked out into
     the activity and are part of the MERGE
     (not sure if this is SVN specific; I can't see how a file/dir
      would be part of the new revision if a working resource had
      not been created for it)
   * we return some props for some resources, and a different set for
     other resources (to keep the wire smaller for now)

   At some point in the future, we'll want to make this "real". Especially
   for proper interoperability.

   #################################################################
*/

typedef struct {
  apr_pool_t *pool;
  ap_filter_t *output;
  apr_bucket_brigade *bb;
  svn_fs_root_t *root;
  const dav_svn_repos *repos;

  struct mr_baton *root_baton;

} merge_response_ctx;

typedef struct mr_baton {
  merge_response_ctx *mrc;

  /* for directories, this is a subpool. otherwise, the pool to use. */
  apr_pool_t *pool;

  /* path for this baton's corresponding FS object */
  const char *path;

  /* for a directory, have we seen a change yet? */
  svn_boolean_t seen_change;

} mr_baton;



/* -------------------------------------------------------------------------

   PRIVATE HELPER FUNCTIONS
*/

static mr_baton *make_child_baton(mr_baton *parent, const char *name,
                                  svn_boolean_t is_dir)
{
  apr_pool_t *pool;
  mr_baton *subdir;

  if (is_dir)
    pool = svn_pool_create(parent->mrc->pool);
  else
    pool = parent->mrc->pool;

  subdir = apr_pcalloc(pool, sizeof(*subdir));
  subdir->mrc = parent->mrc;
  subdir->pool = pool;

  if (parent->path[1] == '\0')  /* must be "/" */
    subdir->path = apr_pstrcat(pool, "/", name, NULL);
  else
    subdir->path = apr_pstrcat(pool, parent->path, "/", name, NULL);

  return subdir;
}

/* send a response to the client for this baton */
static svn_error_t *send_response(mr_baton *baton, svn_boolean_t is_dir)
{
  merge_response_ctx *mrc = baton->mrc;
  const char *href;
  const char *rt;
  svn_fs_id_t *id;
  svn_string_t *stable_id;
  const char *vsn_url;
  apr_status_t status;

  href = dav_svn_build_uri(mrc->repos, DAV_SVN_BUILD_URI_PUBLIC,
                           SVN_IGNORED_REVNUM, baton->path,
                           0 /* add_href */, baton->pool);

  rt = is_dir
    ? "<D:resourcetype><D:collection/></D:resourcetype>" DEBUG_CR
    : "<D:resourcetype/>" DEBUG_CR;

  SVN_ERR( svn_fs_node_id(&id, mrc->root, baton->path, baton->pool) );

  stable_id = svn_fs_unparse_id(id, baton->pool);
  svn_string_appendcstr(stable_id, baton->path);

  vsn_url = dav_svn_build_uri(mrc->repos, DAV_SVN_BUILD_URI_VERSION,
                              SVN_INVALID_REVNUM, stable_id->data,
                              0 /* add_href */, baton->pool);

  status = ap_fputstrs(mrc->output, mrc->bb,
                       "<D:response>" DEBUG_CR
                       "<D:href>", href, "</D:href>" DEBUG_CR
                       "<D:propstat><D:prop>" DEBUG_CR,
                       rt,
                       "<D:checked-in><D:href>",
                       vsn_url,
                       "</D:href></D:checked-in>" DEBUG_CR
                       "<D:status>200 OK</D:status>" DEBUG_CR
                       "</D:prop></D:propstat>" DEBUG_CR
                       "</D:response>" DEBUG_CR,
                       NULL);

  /* ### this is bogus. ap_fputstrs() returns # chars, not a status...
     ### however, it *should*, so let's leave this here for now. */
#if 0
  if (status != APR_SUCCESS)
    return svn_error_create(status, 0, NULL, baton->pool,
                            "could not write response to output");
#endif
  return APR_SUCCESS;
}


/* -------------------------------------------------------------------------

   EDITOR FUNCTIONS
*/

static svn_error_t *mr_replace_root(void *edit_baton,
                                    svn_revnum_t base_revision,
                                    void **root_baton)
{
  merge_response_ctx *mrc = edit_baton;
  apr_pool_t *pool = svn_pool_create(mrc->pool);
  mr_baton *b = apr_pcalloc(pool, sizeof(*b));

  /* note that we create a subpool; the root_baton is passed to the
     close_directory callback, where we will destroy the pool. */

  b->mrc = mrc;
  b->pool = pool;
  b->path = "/";

  mrc->root_baton = b;
  *root_baton = b;
  return NULL;
}

static svn_error_t *mr_delete_entry(svn_string_t *name,
                                    void *parent_baton)
{
  mr_baton *parent = parent_baton;

  /* Removing an item is an explicit change to the parent. Mark it so the
     client will get the data on the new parent. */
  parent->seen_change = TRUE;

  return NULL;
}

static svn_error_t *mr_add_directory(svn_string_t *name,
                                     void *parent_baton,
                                     svn_string_t *copyfrom_path,
                                     svn_revnum_t copyfrom_revision,
                                     void **child_baton)
{
  mr_baton *parent = parent_baton;
  mr_baton *subdir = make_child_baton(parent, name->data, TRUE);

  /* pretend that we've already seen a change for this dir (so that a prop
     change won't generate a second response) */
  subdir->seen_change = TRUE;

  /* the response for this directory will occur at close_directory time */

  /* Adding a subdir is an explicit change to the parent. Mark it so the
     client will get the data on the new parent. */
  parent->seen_change = TRUE;

  *child_baton = subdir;
  return NULL;
}

static svn_error_t *mr_replace_directory(svn_string_t *name,
                                         void *parent_baton,
                                         svn_revnum_t base_revision,
                                         void **child_baton)
{
  mr_baton *parent = parent_baton;
  mr_baton *subdir = make_child_baton(parent, name->data, TRUE);

  /* Don't issue a response until we see a prop change, or a file/subdir
     is added/removed inside this directory. */

  *child_baton = subdir;
  return NULL;
}

static svn_error_t *mr_change_dir_prop(void *dir_baton,
                                       svn_string_t *name,
                                       svn_string_t *value)
{
  mr_baton *dir = dir_baton;

  /* okay, this qualifies as a change, and we need to tell the client
     (which happens at close_directory time). */
  dir->seen_change = TRUE;

  return NULL;
}

static svn_error_t *mr_close_directory(void *dir_baton)
{
  mr_baton *dir = dir_baton;

  /* if we ever saw a change for this directory, then issue a response
     for it. */
  if (dir->seen_change)
    {
      SVN_ERR( send_response(dir, TRUE /* is_dir */) );
    }

  svn_pool_destroy(dir->pool);

  return NULL;
}

static svn_error_t *mr_add_file(svn_string_t *name,
                                void *parent_baton,
                                svn_string_t *copy_path,
                                svn_revnum_t copy_revision,
                                void **file_baton)
{
  mr_baton *parent = parent_baton;
  mr_baton *file = make_child_baton(parent, name->data, FALSE);

  /* We wait until close_file to issue a response for this. */

  /* Adding a file is an explicit change to the parent. Mark it so the
     client will get the data on the new parent. */
  parent->seen_change = TRUE;

  *file_baton = file;
  return NULL;
}

static svn_error_t *mr_replace_file(svn_string_t *name,
                                    void *parent_baton,
                                    svn_revnum_t base_revision,
                                    void **file_baton)
{
  mr_baton *parent = parent_baton;
  mr_baton *file = make_child_baton(parent, name->data, FALSE);

  /* We wait until close_file to issue a response for this. */

  *file_baton = file;
  return NULL;
}

static svn_error_t *mr_close_file(void *file_baton)
{
  /* nothing to do except for sending the response. */
  return send_response(file_baton, FALSE /* is_dir */);
}


/* -------------------------------------------------------------------------

   PUBLIC FUNCTIONS
*/

dav_error * dav_svn__merge_response(ap_filter_t *output,
                                    const dav_svn_repos *repos,
                                    svn_revnum_t new_rev,
                                    apr_xml_elem *prop_elem,
                                    apr_pool_t *pool)
{
  apr_bucket_brigade *bb;
  svn_fs_root_t *committed_root;
  svn_fs_root_t *previous_root;
  svn_error_t *serr;
  const char *vcc;
  char revbuf[20];      /* long enough for %ld */
  apr_hash_t *revs;
  svn_revnum_t *rev_ptr;
  svn_delta_edit_fns_t *editor;
  merge_response_ctx mrc = { 0 };

  serr = svn_fs_revision_root(&committed_root, repos->fs, new_rev, pool);
  if (serr != NULL)
    {
      return dav_svn_convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
                                 "Could not open the FS root for the "
                                 "revision just committed.");
    }
  serr = svn_fs_revision_root(&previous_root, repos->fs, new_rev - 1, pool);
  if (serr != NULL)
    {
      return dav_svn_convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
                                 "Could not open the FS root for the "
                                 "previous revision.");
    }

  bb = apr_brigade_create(pool);

  /* prep some strings */
  
  /* the HREF for the baseline is actually the VCC */
  vcc = dav_svn_build_uri(repos, DAV_SVN_BUILD_URI_VCC, SVN_IGNORED_REVNUM,
                          NULL, 0 /* add_href */, pool);

  /* the version-name of the baseline is the revision number */
  sprintf(revbuf, "%ld", new_rev);

  (void) ap_fputstrs(output, bb,
                     DAV_XML_HEADER DEBUG_CR
                     "<D:merge-response xmlns:D=\"DAV:\">" DEBUG_CR
                     "<D:updated-set>" DEBUG_CR

                     /* generate a response for the new baseline */
                     "<D:response>" DEBUG_CR
                     "<D:href>", vcc, "</D:href>" DEBUG_CR
                     "<D:propstat><D:prop>" DEBUG_CR
                     "<D:version-name>", revbuf, "</D:version-name>" DEBUG_CR
                     "<D:status>200 OK</D:status>" DEBUG_CR
                     "</D:prop></D:propstat>" DEBUG_CR
                     "</D:response>" DEBUG_CR,

                     NULL);

  /* Now we need to generate responses for all the resources which changed.
     This is done through a delta of the two roots.

     Note that a directory is not marked when replace_dir is seen (since it
     typically is used just for changing members in that directory); instead,
     we want for a property change (the only reason the client would need to
     fetch a new directory).

     ### we probably should say something about the dirs, so that we can
     ### pass back the new version URL */

  /* ### hrm. needing this hash table feels wonky. */
  revs = apr_hash_make(pool);
  rev_ptr = apr_palloc(pool, sizeof(*rev_ptr));
  *rev_ptr = new_rev - 1;
  apr_hash_set(revs, "", APR_HASH_KEY_STRING, rev_ptr);

  /* set up the editor for the delta process */
  editor = svn_delta_default_editor(pool);
  editor->replace_root = mr_replace_root;
  editor->delete_entry = mr_delete_entry;
  editor->add_directory = mr_add_directory;
  editor->replace_directory = mr_replace_directory;
  editor->change_dir_prop = mr_change_dir_prop;
  editor->close_directory = mr_close_directory;
  editor->add_file = mr_add_file;
  editor->replace_file = mr_replace_file;
  editor->close_file = mr_close_file;

  /* set up the merge response context */
  mrc.pool = pool;
  mrc.output = output;
  mrc.bb = bb;
  mrc.root = committed_root;
  mrc.repos = repos;

  serr = svn_fs_dir_delta(previous_root, "/", revs,
                          committed_root, "/",
                          editor, &mrc, pool);
  if (serr != NULL)
    {
      return dav_svn_convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
                                 "could not process the merge delta.");
    }

  /* we don't need to call close_edit, but we do need to send a response
     for the root if a change was made. */
  if (mrc.root_baton->seen_change)
    {
      serr = send_response(mrc.root_baton, TRUE /* is_dir */);
      if (serr != NULL)
        {
          return dav_svn_convert_err(serr, HTTP_INTERNAL_SERVER_ERROR,
                                     "could not generate response for the "
                                     "root directory.");
        }
    }

  /* wrap up the merge response */
  (void) ap_fputs(output, bb,
                  "</D:updated-set>" DEBUG_CR
                  "</D:merge-response>" DEBUG_CR);

  /* send whatever is left in the brigade */
  (void) ap_pass_brigade(output, bb);

  return NULL;
}


/* ----------------------------------------------------------------
 * local variables:
 * eval: (load-file "../svn-dev.el")
 * end:
 */
