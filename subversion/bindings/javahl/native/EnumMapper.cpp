/**
 * @copyright
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
 * @endcopyright
 *
 * @file EnumMapper.cpp
 * @brief Implementation of the class EnumMapper
 */

#include "svn_types.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "EnumMapper.h"
#include "../include/org_apache_subversion_javahl_CommitItemStateFlags.h"
#include "../include/org_apache_subversion_javahl_NotifyAction.h"
#include "../include/org_apache_subversion_javahl_NotifyStatus.h"
#include "../include/org_apache_subversion_javahl_NodeKind.h"
#include "../include/org_apache_subversion_javahl_Operation.h"
#include "../include/org_apache_subversion_javahl_LockStatus.h"
#include "../include/org_apache_subversion_javahl_StatusKind.h"
#include "../include/org_apache_subversion_javahl_Revision.h"
#include "../include/org_apache_subversion_javahl_ScheduleKind.h"
#include "../include/org_apache_subversion_javahl_ConflictDescriptor_Kind.h"
#include "../include/org_apache_subversion_javahl_ConflictDescriptor_Action.h"
#include "../include/org_apache_subversion_javahl_ConflictDescriptor_Reason.h"
#include "../include/org_apache_subversion_javahl_Depth.h"

/**
 * Map a C commit state flag constant to the Java constant.
 * @param state     the C commit state flage constant
 * @returns the Java constant
 */
jint EnumMapper::mapCommitMessageStateFlags(apr_byte_t flags)
{
  jint jstateFlags = 0;
  if (flags & SVN_CLIENT_COMMIT_ITEM_ADD)
    jstateFlags |=
      org_apache_subversion_javahl_CommitItemStateFlags_Add;
  if (flags & SVN_CLIENT_COMMIT_ITEM_DELETE)
    jstateFlags |=
      org_apache_subversion_javahl_CommitItemStateFlags_Delete;
  if (flags & SVN_CLIENT_COMMIT_ITEM_TEXT_MODS)
    jstateFlags |=
      org_apache_subversion_javahl_CommitItemStateFlags_TextMods;
  if (flags & SVN_CLIENT_COMMIT_ITEM_PROP_MODS)
    jstateFlags |=
      org_apache_subversion_javahl_CommitItemStateFlags_PropMods;
  if (flags & SVN_CLIENT_COMMIT_ITEM_IS_COPY)
    jstateFlags |=
      org_apache_subversion_javahl_CommitItemStateFlags_IsCopy;
  return jstateFlags;
}

/**
 * Map a C notify state constant to the Java constant.
 * @param state     the C notify state constant
 * @returns the Java constant
 */
jint EnumMapper::mapNotifyState(svn_wc_notify_state_t state)
{
  switch(state)
    {
    default:
    case svn_wc_notify_state_inapplicable:
      return org_apache_subversion_javahl_NotifyStatus_inapplicable;

    case svn_wc_notify_state_unknown:
      return org_apache_subversion_javahl_NotifyStatus_unknown;

    case svn_wc_notify_state_unchanged:
      return org_apache_subversion_javahl_NotifyStatus_unchanged;

    case svn_wc_notify_state_missing:
      return org_apache_subversion_javahl_NotifyStatus_missing;

    case svn_wc_notify_state_obstructed:
      return org_apache_subversion_javahl_NotifyStatus_obstructed;

    case svn_wc_notify_state_changed:
      return org_apache_subversion_javahl_NotifyStatus_changed;

    case svn_wc_notify_state_merged:
      return org_apache_subversion_javahl_NotifyStatus_merged;

    case svn_wc_notify_state_conflicted:
      return org_apache_subversion_javahl_NotifyStatus_conflicted;
    }

}

/**
 * Map a C notify action constant to the Java constant.
 * @param state     the C notify action constant
 * @returns the Java constant
 */
jint EnumMapper::mapNotifyAction(svn_wc_notify_action_t action)
{
  // This is a switch to make the Java constants independent from
  // the C constants.
  switch(action)
    {
    case svn_wc_notify_add:
      /* Adding a path to revision control. */
      return org_apache_subversion_javahl_NotifyAction_add;

    case svn_wc_notify_copy:
      /* Copying a versioned path. */
      return org_apache_subversion_javahl_NotifyAction_copy;

    case svn_wc_notify_delete:
      /* Deleting a versioned path. */
      return org_apache_subversion_javahl_NotifyAction_delete;

    case svn_wc_notify_restore:
      /* Restoring a missing path from the pristine text-base. */
      return org_apache_subversion_javahl_NotifyAction_restore;

    case svn_wc_notify_revert:
      /* Reverting a modified path. */
      return org_apache_subversion_javahl_NotifyAction_revert;

    case svn_wc_notify_failed_revert:
      /* A revert operation has failed. */
      return org_apache_subversion_javahl_NotifyAction_failed_revert;

    case svn_wc_notify_resolved:
      /* Resolving a conflict. */
      return org_apache_subversion_javahl_NotifyAction_resolved;

    case svn_wc_notify_status_completed:
      /* The last notification in a status (including status on
       * externals). */
      return org_apache_subversion_javahl_NotifyAction_status_completed;

    case svn_wc_notify_status_external:
      /* Running status on an external module. */
      return org_apache_subversion_javahl_NotifyAction_status_external;

    case svn_wc_notify_skip:
      /* Skipping a path. */
      return org_apache_subversion_javahl_NotifyAction_skip;

    case svn_wc_notify_update_delete:
      /* Got a delete in an update. */
      return org_apache_subversion_javahl_NotifyAction_update_delete;

    case svn_wc_notify_update_add:
      /* Got an add in an update. */
      return org_apache_subversion_javahl_NotifyAction_update_add;

    case svn_wc_notify_update_replace:
      /* Got a replaced in an update. */
      return org_apache_subversion_javahl_NotifyAction_update_replaced;

    case svn_wc_notify_update_update:
      /* Got any other action in an update. */
      return org_apache_subversion_javahl_NotifyAction_update_update;

    case svn_wc_notify_update_completed:
      /* The last notification in an update (including updates of
       * externals). */
      return org_apache_subversion_javahl_NotifyAction_update_completed;

    case svn_wc_notify_update_external:
      /* Updating an external module. */
      return org_apache_subversion_javahl_NotifyAction_update_external;

    case svn_wc_notify_commit_modified:
      /* Committing a modification. */
      return org_apache_subversion_javahl_NotifyAction_commit_modified;

    case svn_wc_notify_commit_added:
      /* Committing an addition. */
      return org_apache_subversion_javahl_NotifyAction_commit_added;

    case svn_wc_notify_commit_deleted:
      /* Committing a deletion. */
      return org_apache_subversion_javahl_NotifyAction_commit_deleted;

    case svn_wc_notify_commit_replaced:
      /* Committing a replacement. */
      return org_apache_subversion_javahl_NotifyAction_commit_replaced;

    case svn_wc_notify_commit_postfix_txdelta:
      /* Transmitting post-fix text-delta data for a file. */
      return org_apache_subversion_javahl_NotifyAction_commit_postfix_txdelta;

    case svn_wc_notify_blame_revision:
      /* Processed a single revision's blame. */
      return org_apache_subversion_javahl_NotifyAction_blame_revision;

    case svn_wc_notify_locked:
      /* Lock a path */
      return org_apache_subversion_javahl_NotifyAction_locked;

    case svn_wc_notify_unlocked:
      /* Unlock a path */
      return org_apache_subversion_javahl_NotifyAction_unlocked;

    case svn_wc_notify_failed_lock:
      /* Lock failed */
      return org_apache_subversion_javahl_NotifyAction_failed_lock;

    case svn_wc_notify_failed_unlock:
      /* Unlock failed */
      return org_apache_subversion_javahl_NotifyAction_failed_unlock;

    case svn_wc_notify_exists:
      /* Tried adding a path that already exists. */
      return org_apache_subversion_javahl_NotifyAction_exists;

    case svn_wc_notify_changelist_set:
      /* Changelist name set. */
      return org_apache_subversion_javahl_NotifyAction_changelist_set;

    case svn_wc_notify_changelist_clear:
      /* Changelist name cleared. */
      return org_apache_subversion_javahl_NotifyAction_changelist_clear;

    case svn_wc_notify_merge_begin:
      /* A merge operation has begun. */
      return org_apache_subversion_javahl_NotifyAction_merge_begin;

    case svn_wc_notify_foreign_merge_begin:
      /* A merge operation from a foreign repository has begun. */
      return org_apache_subversion_javahl_NotifyAction_foreign_merge_begin;

    case svn_wc_notify_property_added:
      /* Property added */
      return org_apache_subversion_javahl_NotifyAction_property_added;

    case svn_wc_notify_property_modified:
      /* Property modified */
      return org_apache_subversion_javahl_NotifyAction_property_modified;

    case svn_wc_notify_property_deleted:
      /* Property deleted */
      return org_apache_subversion_javahl_NotifyAction_property_deleted;

    case svn_wc_notify_property_deleted_nonexistent:
      /* Property deleted nonexistent */
      return org_apache_subversion_javahl_NotifyAction_property_deleted_nonexistent;

    case svn_wc_notify_revprop_set:
      /* Revision property set */
      return org_apache_subversion_javahl_NotifyAction_revprop_set;

    case svn_wc_notify_revprop_deleted:
      /* Revision property deleted */
      return org_apache_subversion_javahl_NotifyAction_revprop_deleted;

    case svn_wc_notify_merge_completed:
      /* Final notification in a merge */
      return org_apache_subversion_javahl_NotifyAction_merge_completed;

    case svn_wc_notify_tree_conflict:
      /* The path is a tree-conflict victim of the intended action */
      return org_apache_subversion_javahl_NotifyAction_tree_conflict;

    default:
      return -1;
    }
}

/**
 * Map a C node kind constant to the Java constant.
 * @param state     the C node kind constant
 * @returns the Java constant
 */
jint EnumMapper::mapNodeKind(svn_node_kind_t nodeKind)
{
  switch(nodeKind)
    {
    case svn_node_none:
      return org_apache_subversion_javahl_NodeKind_none;

    case svn_node_file:
      return org_apache_subversion_javahl_NodeKind_file;

    case svn_node_dir:
      return org_apache_subversion_javahl_NodeKind_dir;

    case svn_node_unknown:
      return org_apache_subversion_javahl_NodeKind_unknown;

    default:
      return org_apache_subversion_javahl_NodeKind_unknown;
    }
}

/**
 * Map a C notify lock state constant to the Java constant.
 * @param state     the C notify lock state constant
 * @returns the Java constant
 */
jint EnumMapper::mapNotifyLockState(svn_wc_notify_lock_state_t state)
{
  switch(state)
    {
    case svn_wc_notify_lock_state_inapplicable:
      return org_apache_subversion_javahl_LockStatus_inapplicable;

    case svn_wc_notify_lock_state_unknown:
      return org_apache_subversion_javahl_LockStatus_unknown;

    case svn_wc_notify_lock_state_unchanged:
      return org_apache_subversion_javahl_LockStatus_unchanged;

    case svn_wc_notify_lock_state_locked:
      return org_apache_subversion_javahl_LockStatus_locked;

    case svn_wc_notify_lock_state_unlocked:
      return org_apache_subversion_javahl_LockStatus_unlocked;

    default:
      return org_apache_subversion_javahl_LockStatus_inapplicable;
    }
}

/**
 * Map a C wc schedule constant to the Java constant.
 * @param state     the C wc schedule constant
 * @returns the Java constant
 */
jint EnumMapper::mapScheduleKind(svn_wc_schedule_t schedule)
{
  switch(schedule)
    {
      /** Nothing special here */
    case svn_wc_schedule_normal:
      return org_apache_subversion_javahl_ScheduleKind_normal;

      /** Slated for addition */
    case svn_wc_schedule_add:
      return org_apache_subversion_javahl_ScheduleKind_add;

      /** Slated for deletion */
    case svn_wc_schedule_delete:
      return org_apache_subversion_javahl_ScheduleKind_delete;

      /** Slated for replacement (delete + add) */
    case svn_wc_schedule_replace:
      return org_apache_subversion_javahl_ScheduleKind_replace;

    default:
      return org_apache_subversion_javahl_ScheduleKind_normal;
    }
}

/**
 * Map a C wc state constant to the Java constant.
 * @param state     the C wc state constant
 * @returns the Java constant
 */
jint EnumMapper::mapStatusKind(svn_wc_status_kind svnKind)
{
  switch(svnKind)
    {
    case svn_wc_status_none:
    default:
      return org_apache_subversion_javahl_StatusKind_none;

    case svn_wc_status_unversioned:
      return org_apache_subversion_javahl_StatusKind_unversioned;

    case svn_wc_status_normal:
      return org_apache_subversion_javahl_StatusKind_normal;

    case svn_wc_status_added:
      return org_apache_subversion_javahl_StatusKind_added;

    case svn_wc_status_missing:
      return org_apache_subversion_javahl_StatusKind_missing;

    case svn_wc_status_deleted:
      return org_apache_subversion_javahl_StatusKind_deleted;

    case svn_wc_status_replaced:
      return org_apache_subversion_javahl_StatusKind_replaced;

    case svn_wc_status_modified:
      return org_apache_subversion_javahl_StatusKind_modified;

    case svn_wc_status_merged:
      return org_apache_subversion_javahl_StatusKind_merged;

    case svn_wc_status_conflicted:
      return org_apache_subversion_javahl_StatusKind_conflicted;

    case svn_wc_status_ignored:
      return org_apache_subversion_javahl_StatusKind_ignored;

    case svn_wc_status_obstructed:
      return org_apache_subversion_javahl_StatusKind_obstructed;

    case svn_wc_status_external:
      return org_apache_subversion_javahl_StatusKind_external;

    case svn_wc_status_incomplete:
      return org_apache_subversion_javahl_StatusKind_incomplete;
    }
}

jint EnumMapper::mapConflictKind(svn_wc_conflict_kind_t kind)
{
  switch (kind)
    {
    case svn_wc_conflict_kind_text:
    default:
      return org_apache_subversion_javahl_ConflictDescriptor_Kind_text;

    case svn_wc_conflict_kind_property:
      return org_apache_subversion_javahl_ConflictDescriptor_Kind_property;
    }
}

jint EnumMapper::mapConflictAction(svn_wc_conflict_action_t action)
{
  switch (action)
    {
    case svn_wc_conflict_action_edit:
    default:
      return org_apache_subversion_javahl_ConflictDescriptor_Action_edit;

    case svn_wc_conflict_action_add:
      return org_apache_subversion_javahl_ConflictDescriptor_Action_add;

    case svn_wc_conflict_action_delete:
      return org_apache_subversion_javahl_ConflictDescriptor_Action_delete;
    }
}

jint EnumMapper::mapConflictReason(svn_wc_conflict_reason_t reason)
{
  switch (reason)
    {
    case svn_wc_conflict_reason_edited:
    default:
      return org_apache_subversion_javahl_ConflictDescriptor_Reason_edited;

    case svn_wc_conflict_reason_obstructed:
      return org_apache_subversion_javahl_ConflictDescriptor_Reason_obstructed;

    case svn_wc_conflict_reason_deleted:
      return org_apache_subversion_javahl_ConflictDescriptor_Reason_deleted;

    case svn_wc_conflict_reason_missing:
      return org_apache_subversion_javahl_ConflictDescriptor_Reason_missing;

    case svn_wc_conflict_reason_unversioned:
      return org_apache_subversion_javahl_ConflictDescriptor_Reason_unversioned;

    case svn_wc_conflict_reason_added:
      return org_apache_subversion_javahl_ConflictDescriptor_Reason_added;
    }
}

jint EnumMapper::mapDepth(svn_depth_t depth)
{
  switch (depth)
    {
    case svn_depth_unknown:
    default:
      return org_apache_subversion_javahl_Depth_unknown;

    case svn_depth_exclude:
      return org_apache_subversion_javahl_Depth_exclude;

    case svn_depth_empty:
      return org_apache_subversion_javahl_Depth_empty;

    case svn_depth_files:
      return org_apache_subversion_javahl_Depth_files;

    case svn_depth_immediates:
      return org_apache_subversion_javahl_Depth_immediates;

    case svn_depth_infinity:
      return org_apache_subversion_javahl_Depth_infinity;
    }
}

jint EnumMapper::mapOperation(svn_wc_operation_t operation)
{
  switch (operation)
    {
    case svn_wc_operation_none:
    default:
      return org_apache_subversion_javahl_Operation_none;
    case svn_wc_operation_update:
      return org_apache_subversion_javahl_Operation_update;
    case svn_wc_operation_switch:
      return org_apache_subversion_javahl_Operation_switched;
    case svn_wc_operation_merge:
      return org_apache_subversion_javahl_Operation_merge;
    }
}
