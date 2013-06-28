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
 * @file RemoteSessionContext.cpp
 * @brief Implementation of the class RemoteSessionContext
 */


#include "RemoteSessionContext.h"
#include "JNIUtil.h"
#include "Prompter.h"

#define STRING_RETURN_SIGNATURE "()Ljava/lang/String;"

RemoteSessionContext::RemoteSessionContext(
    jobject contextHolder, SVN::Pool &pool,
    const char* configDirectory, jobject jconfigHandler,
    const char*  usernameStr, const char*  passwordStr,
    Prompter* prompter, jobject jprogress)
  : OperationContext(pool), m_raCallbacks(NULL)
{
  setConfigDirectory(configDirectory);
  setConfigCallback(jconfigHandler);
  if (usernameStr != NULL)
    username(usernameStr);

  if (passwordStr != NULL)
    password(passwordStr);

  setPrompt(prompter);

  /*
   * Attach session context java object
   */
  static jfieldID ctxFieldID = 0;
  attachJavaObject(contextHolder,
      "L"JAVA_PACKAGE"/remote/RemoteSession$RemoteSessionContext;",
      "sessionContext", &ctxFieldID);

  /*
   * Set the progress callback
   */
  JNIEnv *env = JNIUtil::getEnv();

  jclass clazz = env->GetObjectClass(m_jctx);
  if (JNIUtil::isJavaExceptionThrown())
    return;

  jmethodID mid = env->GetMethodID(
      clazz, "setProgressCallback",
      "(L"JAVA_PACKAGE"/callback/ProgressCallback;)V");
  if (JNIUtil::isJavaExceptionThrown() || mid == 0)
    return;

  env->CallVoidMethod(m_jctx, mid, jprogress);
  env->DeleteLocalRef(jprogress);

  /*
   * Setup callbacks
   */
  SVN_JNI_ERR(svn_ra_create_callbacks(&m_raCallbacks, m_pool->getPool()), );

  m_raCallbacks->auth_baton = getAuthBaton(pool);
  m_raCallbacks->cancel_func = checkCancel;
  m_raCallbacks->get_client_string = clientName;
  m_raCallbacks->progress_baton = m_jctx;
  m_raCallbacks->progress_func = progress;

  /*
   * JNI RA layer does not work with WC so all WC callbacks are set to NULL
   */
  m_raCallbacks->get_wc_prop = NULL;
  m_raCallbacks->invalidate_wc_props = NULL;
  m_raCallbacks->push_wc_prop = NULL;
  m_raCallbacks->set_wc_prop = NULL;

  /*
   * Don't set deprecated callback
   */
  m_raCallbacks->open_tmp_file = NULL;
}

RemoteSessionContext::~RemoteSessionContext()
{
}

void *
RemoteSessionContext::getCallbackBaton()
{
  return this;
}

svn_ra_callbacks2_t *
RemoteSessionContext::getCallbacks()
{
  return m_raCallbacks;
}
