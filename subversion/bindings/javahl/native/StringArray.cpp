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
 * @file StringArray.cpp
 * @brief Implementation of the class StringArray
 */

#include "StringArray.h"
#include "JNIUtil.h"
#include "JNIStringHolder.h"
#include <apr_tables.h>
#include <apr_strings.h>
#include "svn_path.h"
#include <iostream>

StringArray::~StringArray()
{
  if (m_stringArray != NULL)
    JNIUtil::getEnv()->DeleteLocalRef(m_stringArray);
  if (m_stringCollection != NULL)
    JNIUtil::getEnv()->DeleteLocalRef(m_stringCollection);
}

const apr_array_header_t *StringArray::array(const SVN::Pool &pool)
{
  apr_array_header_t *strings
    = apr_array_make(pool.pool(), m_strings.size(), sizeof(char *));

  std::vector<std::string>::const_iterator it;
  for (it = m_strings.begin(); it < m_strings.end(); ++it)
    {
      APR_ARRAY_PUSH(strings, const char *) = it->c_str();
      if (JNIUtil::isExceptionThrown())
        return NULL;
    }

  return strings;
}

StringArray::StringArray(jobjectArray jstrings)
{
  m_stringArray = jstrings;
  m_stringCollection = NULL;

  if (jstrings != NULL)
    {
      JNIEnv *env = JNIUtil::getEnv();
      jint arraySize = env->GetArrayLength(jstrings);
      if (JNIUtil::isExceptionThrown())
        return;

      for (int i = 0; i < arraySize; ++i)
        {
          jobject jstr = env->GetObjectArrayElement(jstrings, i);
          if (JNIUtil::isExceptionThrown())
            return;

          JNIStringHolder str((jstring)jstr);
          if (JNIUtil::isExceptionThrown())
            return;

          m_strings.push_back(std::string((const char *)str));
        }
    }
}

StringArray::StringArray(jobject jstringCollection)
{
  m_stringArray = NULL;
  m_stringCollection = jstringCollection;

  if (jstringCollection != NULL)
    {
      JNIEnv *env = JNIUtil::getEnv();

      jclass clazz = env->FindClass("java/util/Collection");

      static jmethodID mid_toArray = 0;
      if (mid_toArray == 0)
        {
          mid_toArray = env->GetMethodID(clazz, "toArray",
                                         "()[Ljava/lang/Object;");
          if (JNIUtil::isExceptionThrown())
            return;
        }

      jobjectArray jstrings = (jobjectArray) env->CallObjectMethod(
                                               jstringCollection, mid_toArray);

      jint arraySize = env->GetArrayLength(jstrings);
      if (JNIUtil::isExceptionThrown())
        return;

      for (int i = 0; i < arraySize; ++i)
        {
          jobject jstr = env->GetObjectArrayElement(jstrings, i);
          if (JNIUtil::isExceptionThrown())
            return;

          JNIStringHolder str((jstring)jstr);
          if (JNIUtil::isExceptionThrown())
            return;

          m_strings.push_back(std::string((const char *)str));
        }

      JNIUtil::getEnv()->DeleteLocalRef(clazz);
      if (JNIUtil::isExceptionThrown())
        return;

      JNIUtil::getEnv()->DeleteLocalRef(jstrings);
      if (JNIUtil::isExceptionThrown())
        return;
    }
}
