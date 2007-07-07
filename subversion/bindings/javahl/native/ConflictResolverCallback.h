/**
 * @copyright
 * ====================================================================
 * Copyright (c) 2007 CollabNet.  All rights reserved.
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
 * @endcopyright
 *
 * @file ConflictResolverCallback.h
 * @brief Interface of the class ConflictResolverCallback
 */

#ifndef CONFLICT_RESOLVER_CALLBACK_H
#define CONFLICT_RESOLVER_CALLBACK_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <jni.h>
#include "svn_error.h"
#include "svn_wc.h"

/**
 * This class passes conflict resolution events from Subversion to a
 * Java object.
 * @since 1.5
 */
class ConflictResolverCallback
{
 private:
  /**
   * The Java object which handles the conflict resolution
   * events. This is a global reference, because it has to live longer
   * than the SVNClient.onProgress() call.
   */
  jobject m_conflictResolver;

  /**
   * Create a new instance, storing a global reference to the
   * corresponding Java object.
   *
   * @param jconflictResolver Reference to the Java peer.
   */
  ConflictResolverCallback(jobject jconflictResolver);

 public:
  /**
   * Destroy the instance, and delete the global reference to the
   * Java object.
   */
  ~ConflictResolverCallback();

  /** Constructor function called from C JNI glue code. */
  static ConflictResolverCallback *
  makeCConflictResolverCallback(jobject jconflictResolver);

  /**
   * Implementation of the svn_wc_conflict_resolver_func_t API.
   */
  static svn_error_t *
  resolveConflict(svn_wc_conflict_result_t *result,
                  const svn_wc_conflict_description_t *desc,
                  void *baton,
                  apr_pool_t *pool);

 protected:
  /**
   * Delegates to m_conflictResolver.resolve(), which provides the
   * logic for the implementation of the svn_wc_conflict_resolver_func_t
   * API.
   */
  svn_error_t * resolve(svn_wc_conflict_result_t *result,
                        const svn_wc_conflict_description_t *desc,
                        apr_pool_t *pool);
};

#endif  // CONFLICT_RESOLVER_CALLBACK_H
