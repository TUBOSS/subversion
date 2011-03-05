/*
 * cache.h: cache vtable interface
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

#ifndef SVN_LIBSVN_SUBR_CACHE_H
#define SVN_LIBSVN_SUBR_CACHE_H

#include "private/svn_cache.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct svn_cache__vtable_t {
  svn_error_t *(*get)(void **value,
                      svn_boolean_t *found,
                      void *cache_implementation,
                      const void *key,
                      apr_pool_t *pool);

  svn_error_t *(*set)(void *cache_implementation,
                      const void *key,
                      void *value,
                      apr_pool_t *pool);

  svn_error_t *(*iter)(svn_boolean_t *completed,
                       void *cache_implementation,
                       svn_iter_apr_hash_cb_t func,
                       void *baton,
                       apr_pool_t *pool);

  svn_boolean_t (*is_cachable)(void *cache_implementation,
                               apr_size_t size);
} svn_cache__vtable_t;

struct svn_cache__t {
  const svn_cache__vtable_t *vtable;
  svn_cache__error_handler_t error_handler;
  void *error_baton;
  void *cache_internal;
};


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_SUBR_CACHE_H */
