/*
 * tree_conflicts.c: Storage of tree conflict descriptions in the WC.
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

#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_types.h"
#include "svn_pools.h"

#include "tree_conflicts.h"
#include "log.h"
#include "entries.h"
#include "lock.h"
#include "wc.h"

#include "private/svn_skel.h"
#include "private/svn_wc_private.h"
#include "private/svn_token.h"

#include "svn_private_config.h"


/* OVERVIEW
 *
 * This file handles the storage and retrieval of tree conflict descriptions
 * (svn_wc_conflict_description_t) in the WC.
 *
 * Data Format
 *
 * All tree conflicts descriptions for the current tree conflict victims in
 * one parent directory are stored in a single "tree_conflict_data" text
 * field in that parent's THIS_DIR entry.
 *
 *   tree_conflict_data: zero or more conflicts (one per victim path),
 *     separated by the SVN_WC__TREE_CONFLICT_DESC_SEPARATOR character.
 *
 *   a description entry: a fixed sequence of text fields, some of which
 *     may be empty, corresponding to the pertinent fields of
 *     svn_wc_conflict_description_t, separated by
 *     SVN_WC__TREE_CONFLICT_DESC_FIELD_SEPARATOR.
 *
 *   a field: a string within which any separator or escape characters are
 *     escaped with the escape character SVN_WC__TREE_CONFLICT_ESCAPE_CHAR.
 *
 * Error Handling
 *
 * On reading from the WC entry, errors of malformed data are handled by
 * raising an svn_error_t, as these can occur from WC corruption. On
 * writing, errors in the internal data consistency before it is written are
 * handled more severely because any such errors must be due to a bug.
 */


/* ### this should move to a more general location...  */
/* A map for svn_node_kind_t values. */
static const svn_token_map_t node_kind_map[] =
{
  { "none", svn_node_none },
  { "file", svn_node_file },
  { "dir",  svn_node_dir },
  { "",     svn_node_unknown },
  { NULL }
};

/* A map for svn_wc_operation_t values. */
const svn_token_map_t svn_wc__operation_map[] =
{
  { "none",   svn_wc_operation_none },
  { "update", svn_wc_operation_update },
  { "switch", svn_wc_operation_switch },
  { "merge",  svn_wc_operation_merge },
  { NULL }
};

/* A map for svn_wc_conflict_action_t values. */
const svn_token_map_t svn_wc__conflict_action_map[] =
{
  { "edited",   svn_wc_conflict_action_edit },
  { "deleted",  svn_wc_conflict_action_delete },
  { "added",    svn_wc_conflict_action_add },
  { "replaced", svn_wc_conflict_action_replace },
  { NULL }
};

/* A map for svn_wc_conflict_reason_t values. */
const svn_token_map_t svn_wc__conflict_reason_map[] =
{
  { "edited",      svn_wc_conflict_reason_edited },
  { "deleted",     svn_wc_conflict_reason_deleted },
  { "missing",     svn_wc_conflict_reason_missing },
  { "obstructed",  svn_wc_conflict_reason_obstructed },
  { "added",       svn_wc_conflict_reason_added },
  { "replaced",    svn_wc_conflict_reason_replaced },
  { "unversioned", svn_wc_conflict_reason_unversioned },
  { NULL }
};


/* */
static svn_boolean_t
is_valid_version_info_skel(const svn_skel_t *skel)
{
  return (svn_skel__list_length(skel) == 5
          && svn_skel__matches_atom(skel->children, "version")
          && skel->children->next->is_atom
          && skel->children->next->next->is_atom
          && skel->children->next->next->next->is_atom
          && skel->children->next->next->next->next->is_atom);
}


/* */
static svn_boolean_t
is_valid_conflict_skel(const svn_skel_t *skel)
{
  int i;

  if (svn_skel__list_length(skel) != 8
      || !svn_skel__matches_atom(skel->children, "conflict"))
    return FALSE;

  /* 5 atoms ... */
  skel = skel->children->next;
  for (i = 5; i--; skel = skel->next)
    if (!skel->is_atom)
      return FALSE;

  /* ... and 2 version info skels. */
  return (is_valid_version_info_skel(skel)
          && is_valid_version_info_skel(skel->next));
}

/* Parse the enumeration value in VALUE into a plain
 * 'int', using MAP to convert from strings to enumeration values.
 * In MAP, a null .str field marks the end of the map.
 */
static svn_error_t *
read_enum_field(int *result,
                const svn_token_map_t *map,
                const svn_skel_t *skel)
{
  int value = svn_token__from_mem(map, skel->data, skel->len);

  if (value == SVN_TOKEN_UNKNOWN)
    return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
                            _("Unknown enumeration value in tree conflict "
                              "description"));

  *result = value;
  return SVN_NO_ERROR;
}

/* Parse the conflict info fields from SKEL into *VERSION_INFO. */
static svn_error_t *
read_node_version_info(const svn_wc_conflict_version_t **version_info,
                       const svn_skel_t *skel,
                       apr_pool_t *result_pool,
                       apr_pool_t *scratch_pool)
{
  int n;
  const char *repos_root;
  const char *repos_relpath;
  svn_revnum_t peg_rev;
  svn_node_kind_t kind;

  if (!is_valid_version_info_skel(skel))
    return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
                            _("Invalid version info in tree conflict "
                              "description"));

  repos_root = apr_pstrmemdup(result_pool,
                                           skel->children->next->data,
                                           skel->children->next->len);
  if (*repos_root == '\0')
    {
      *version_info = NULL;
      return SVN_NO_ERROR;
    }

  peg_rev = SVN_STR_TO_REV(apr_pstrmemdup(scratch_pool,
                                          skel->children->next->next->data,
                                          skel->children->next->next->len));

  repos_relpath = apr_pstrmemdup(result_pool,
                                 skel->children->next->next->next->data,
                                 skel->children->next->next->next->len);

  SVN_ERR(read_enum_field(&n, node_kind_map,
                          skel->children->next->next->next->next));
  kind = (svn_node_kind_t)n;

  *version_info = svn_wc_conflict_version_create(repos_root,
                                                 repos_relpath,
                                                 peg_rev,
                                                 kind,
                                                 result_pool);

  return SVN_NO_ERROR;
}

/* Parse a newly allocated svn_wc_conflict_description2_t object from the
 * character string pointed to by *START. Return the result in *CONFLICT.
 * Don't read further than END. Set *START to point to the next character
 * after the description that was read.
 * DIR_PATH is the path to the WC directory whose conflicts are being read.
 * Do all allocations in pool.
 */
svn_error_t *
svn_wc__deserialize_conflict(const svn_wc_conflict_description2_t **conflict,
                             const svn_skel_t *skel,
                             const char *dir_path,
                             apr_pool_t *result_pool,
                             apr_pool_t *scratch_pool)
{
  const char *victim_basename;
  const char *victim_abspath;
  svn_node_kind_t node_kind;
  svn_wc_operation_t operation;
  svn_wc_conflict_action_t action;
  svn_wc_conflict_reason_t reason;
  const svn_wc_conflict_version_t *src_left_version;
  const svn_wc_conflict_version_t *src_right_version;
  int n;
  svn_wc_conflict_description2_t *new_conflict;

  if (!is_valid_conflict_skel(skel))
    return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
                            _("Invalid conflict info in tree conflict "
                              "description"));

  /* victim basename */
  victim_basename = apr_pstrmemdup(scratch_pool,
                                   skel->children->next->data,
                                   skel->children->next->len);
  if (victim_basename[0] == '\0')
    return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
                            _("Empty 'victim' field in tree conflict "
                              "description"));

  /* node_kind */
  SVN_ERR(read_enum_field(&n, node_kind_map, skel->children->next->next));
  node_kind = (svn_node_kind_t)n;
  if (node_kind != svn_node_file && node_kind != svn_node_dir)
    return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
             _("Invalid 'node_kind' field in tree conflict description"));

  /* operation */
  SVN_ERR(read_enum_field(&n, svn_wc__operation_map,
                          skel->children->next->next->next));
  operation = (svn_wc_operation_t)n;

  SVN_ERR(svn_dirent_get_absolute(&victim_abspath,
                    svn_dirent_join(dir_path, victim_basename, scratch_pool),
                    scratch_pool));

  /* action */
  SVN_ERR(read_enum_field(&n, svn_wc__conflict_action_map,
                          skel->children->next->next->next->next));
  action = n;

  /* reason */
  SVN_ERR(read_enum_field(&n, svn_wc__conflict_reason_map,
                          skel->children->next->next->next->next->next));
  reason = n;

  /* Let's just make it a bit easier on ourself here... */
  skel = skel->children->next->next->next->next->next->next;

  /* src_left_version */
  SVN_ERR(read_node_version_info(&src_left_version, skel,
                                 result_pool, scratch_pool));

  /* src_right_version */
  SVN_ERR(read_node_version_info(&src_right_version, skel->next,
                                 result_pool, scratch_pool));

  new_conflict = svn_wc_conflict_description_create_tree2(victim_abspath,
    node_kind, operation, src_left_version, src_right_version,
    result_pool);
  new_conflict->action = action;
  new_conflict->reason = reason;

  *conflict = new_conflict;
  return SVN_NO_ERROR;
}


/* ### this is BAD. the CONFLICTS structure should not be dependent upon
   ### DIR_PATH. each conflict should be labeled with an entry name, not
   ### a whole path. (and a path which happens to vary based upon invocation
   ### of the user client and these APIs)  */
svn_error_t *
svn_wc__read_tree_conflicts(apr_hash_t **conflicts,
                            const char *conflict_data,
                            const char *dir_path,
                            apr_pool_t *pool)
{
  const svn_skel_t *skel;
  apr_pool_t *iterpool;

  *conflicts = apr_hash_make(pool);

  if (conflict_data == NULL)
    return SVN_NO_ERROR;

  skel = svn_skel__parse(conflict_data, strlen(conflict_data), pool);
  if (skel == NULL)
    return svn_error_create(SVN_ERR_WC_CORRUPT, NULL,
                            _("Error parsing tree conflict skel"));

  iterpool = svn_pool_create(pool);
  for (skel = skel->children; skel != NULL; skel = skel->next)
    {
      const svn_wc_conflict_description2_t *conflict;

      svn_pool_clear(iterpool);
      SVN_ERR(svn_wc__deserialize_conflict(&conflict, skel, dir_path,
                                           pool, iterpool));
      if (conflict != NULL)
        apr_hash_set(*conflicts, svn_dirent_basename(conflict->local_abspath,
                                                     pool),
                     APR_HASH_KEY_STRING, conflict);
    }
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Prepend to SKEL the string corresponding to enumeration value N, as found
 * in MAP. */
static svn_error_t *
skel_prepend_enum(svn_skel_t *skel,
                  const svn_token_map_t *map,
                  int n,
                  apr_pool_t *result_pool)
{
  svn_skel__prepend(svn_skel__str_atom(svn_token__to_word(map, n),
                                       result_pool), skel);
  return SVN_NO_ERROR;
}

/* Prepend to PARENT_SKEL the several fields that represent VERSION_INFO, */
static svn_error_t *
prepend_version_info_skel(svn_skel_t *parent_skel,
                          const svn_wc_conflict_version_t *version_info,
                          apr_pool_t *pool)
{
  svn_skel_t *skel = svn_skel__make_empty_list(pool);

  /* node_kind */
  SVN_ERR(skel_prepend_enum(skel, node_kind_map, version_info->node_kind,
                            pool));

  /* path_in_repos */
  svn_skel__prepend(svn_skel__str_atom(version_info->path_in_repos
                                       ? version_info->path_in_repos
                                       : "", pool), skel);

  /* peg_rev */
  svn_skel__prepend(svn_skel__str_atom(apr_psprintf(pool, "%ld",
                                                    version_info->peg_rev),
                                       pool), skel);

  /* repos_url */
  svn_skel__prepend(svn_skel__str_atom(version_info->repos_url
                                       ? version_info->repos_url
                                       : "", pool), skel);

  svn_skel__prepend(svn_skel__str_atom("version", pool), skel);

  SVN_ERR_ASSERT(is_valid_version_info_skel(skel));

  svn_skel__prepend(skel, parent_skel);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__serialize_conflict(svn_skel_t **skel,
                           const svn_wc_conflict_description2_t *conflict,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool)
{
  /* A conflict version struct with all fields null/invalid. */
  static const svn_wc_conflict_version_t null_version = {
    NULL, SVN_INVALID_REVNUM, NULL, svn_node_unknown };
  svn_skel_t *c_skel = svn_skel__make_empty_list(result_pool);
  const char *victim_basename;

  /* src_right_version */
  if (conflict->src_right_version)
    SVN_ERR(prepend_version_info_skel(c_skel, conflict->src_right_version,
                                      result_pool));
  else
    SVN_ERR(prepend_version_info_skel(c_skel, &null_version, result_pool));

  /* src_left_version */
  if (conflict->src_left_version)
    SVN_ERR(prepend_version_info_skel(c_skel, conflict->src_left_version,
                                      result_pool));
  else
    SVN_ERR(prepend_version_info_skel(c_skel, &null_version, result_pool));

  /* reason */
  SVN_ERR(skel_prepend_enum(c_skel, svn_wc__conflict_reason_map,
                            conflict->reason, result_pool));

  /* action */
  SVN_ERR(skel_prepend_enum(c_skel, svn_wc__conflict_action_map,
                            conflict->action, result_pool));

  /* operation */
  SVN_ERR(skel_prepend_enum(c_skel, svn_wc__operation_map,
                            conflict->operation, result_pool));

  /* node_kind */
  SVN_ERR_ASSERT(conflict->node_kind == svn_node_dir
                 || conflict->node_kind == svn_node_file);
  SVN_ERR(skel_prepend_enum(c_skel, node_kind_map, conflict->node_kind,
                            result_pool));

  /* Victim path (escaping separator chars). */
  victim_basename = svn_dirent_basename(conflict->local_abspath, result_pool);
  SVN_ERR_ASSERT(strlen(victim_basename) > 0);
  svn_skel__prepend(svn_skel__str_atom(victim_basename, result_pool), c_skel);

  svn_skel__prepend(svn_skel__str_atom("conflict", result_pool), c_skel);

  SVN_ERR_ASSERT(is_valid_conflict_skel(c_skel));

  *skel = c_skel;
  return SVN_NO_ERROR;
}


/*
 * This function could be static, but we need to link to it
 * in a unit test in tests/libsvn_wc/, so it isn't.
 * (and we use it to serialize tree conflicts in log.c :( )
 */
svn_error_t *
svn_wc__write_tree_conflicts(const char **conflict_data,
                             apr_hash_t *conflicts,
                             apr_pool_t *pool)
{
  svn_skel_t *skel = svn_skel__make_empty_list(pool);
  apr_hash_index_t *hi;

  for (hi = apr_hash_first(pool, conflicts); hi; hi = apr_hash_next(hi))
    {
      svn_skel_t *c_skel;

      SVN_ERR(svn_wc__serialize_conflict(&c_skel, svn_apr_hash_index_val(hi),
                                         pool, pool));
      svn_skel__prepend(c_skel, skel);
    }

  *conflict_data = svn_skel__unparse(skel, pool)->data;

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__del_tree_conflict(svn_wc_context_t *wc_ctx,
                          const char *victim_abspath,
                          apr_pool_t *scratch_pool)
{
  SVN_ERR_ASSERT(svn_dirent_is_absolute(victim_abspath));

  SVN_ERR(svn_wc__db_op_set_tree_conflict(wc_ctx->db, victim_abspath,
                                          NULL, scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__add_tree_conflict(svn_wc_context_t *wc_ctx,
                          const svn_wc_conflict_description2_t *conflict,
                          apr_pool_t *scratch_pool)
{
  const svn_wc_conflict_description2_t *existing_conflict;

  /* Re-adding an existing tree conflict victim is an error. */
  SVN_ERR(svn_wc__db_op_read_tree_conflict(&existing_conflict, wc_ctx->db,
                                           conflict->local_abspath,
                                           scratch_pool, scratch_pool));
  if (existing_conflict != NULL)
    return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
                             _("Attempt to add tree conflict that already "
                               "exists at '%s'"),
                             svn_dirent_local_style(conflict->local_abspath,
                                                    scratch_pool));

  return svn_error_return(
    svn_wc__db_op_set_tree_conflict(wc_ctx->db, conflict->local_abspath,
                                          conflict, scratch_pool));
}


svn_error_t *
svn_wc__get_tree_conflict(const svn_wc_conflict_description2_t **tree_conflict,
                          svn_wc_context_t *wc_ctx,
                          const char *victim_abspath,
                          apr_pool_t *result_pool,
                          apr_pool_t *scratch_pool)
{
  SVN_ERR_ASSERT(svn_dirent_is_absolute(victim_abspath));

  return svn_error_return(
    svn_wc__db_op_read_tree_conflict(tree_conflict, wc_ctx->db, victim_abspath,
                                     scratch_pool, scratch_pool));
}
