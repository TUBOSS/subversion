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
 */

#ifndef SVN_JAVAHL_JNIWRAPPER_LIST_HPP
#define SVN_JAVAHL_JNIWRAPPER_LIST_HPP

#include <vector>

#include "jni_env.hpp"
#include "jni_object.hpp"

namespace Java {

/**
 * Non-template base for an immutable type-safe Java list.
 *
 * Converts the list to a @c std::vector of @c jobject references.
 *
 * @since New in 1.9.
 */
class BaseList : public Object
{
  typedef std::vector<jobject> ovector;

public:
  /**
   * Returns the number of elements in the list.
   */
  jint length() const
    {
      return jint(m_contents.size());
    }

protected:
  /**
   * Constructs the list wrapper, converting the contents to an
   * @c std::vector.
   */
  explicit BaseList(Env env, jobject jlist)
    : Object(env, m_class_name, jlist),
      m_contents(convert_to_vector(env, m_class, m_jthis))
    {}

  /**
   * Returns the object reference at @a index.
   */
  jobject operator[](jint index) const
    {
      return m_contents[ovector::size_type(index)];
    }

private:
  static const char* const m_class_name;
  static ovector convert_to_vector(Env env, jclass cls, jobject jlist);
  const ovector m_contents;
};

/**
 * Template wrapper for an immutable type-safe Java list.
 *
 * @since New in 1.9.
 */
template <typename T>
class List : public BaseList
{
public:
  /**
   * Constructs the list wrapper, converting the contents to an
   * @c std::vector.
   */
  explicit List(Env env, jobject jlist)
    : BaseList(env, jlist)
    {}

  /**
   * Returns a wrapper object for the object reference at @a index.
   */
  T operator[](jint index) const
    {
      return T(m_env, BaseList::operator[](index));
    }
};

/**
 * Non-template base for a mutable type-safe Java list.
 *
 * @since New in 1.9.
 */
class BaseMutableList : public Object
{
public:
  /**
   * Clears the contents of the list.
   */
  void clear();

  /**
   * Returns the number of elements in the list.
   */
  jint length() const;

  /**
   * Checks if the list is empty.
   */
  bool is_empty() const
    {
      return (length() == 0);
    }

protected:
  /**
   * Constructs the list wrapper, deriving the class from @a jlist.
   */
  explicit BaseMutableList(Env env, jobject jlist)
    : Object(env, jlist),
      m_mid_add(NULL),
      m_mid_clear(NULL),
      m_mid_get(NULL),
      m_mid_size(NULL)
    {}

  /**
   * Constructs and wraps an empty list of type @c java.util.ArrayList
   * with initial allocation size @a length.
   */
  explicit BaseMutableList(Env env, jint length);

  /**
   * Appends @a obj to the end of the list.
   */
  void add(jobject obj);

  /**
   * Returns the object reference at @a index.
   */
  jobject operator[](jint index) const;

private:
  static const char* const m_class_name;
  jmethodID m_mid_add;
  jmethodID m_mid_clear;
  mutable jmethodID m_mid_get;
  mutable jmethodID m_mid_size;
};

/**
 * Template wrapper for a mutable type-safe Java list.
 *
 * @since New in 1.9.
 */
template <typename T>
class MutableList : public BaseMutableList
{
public:
  /**
   * Constructs the list wrapper, deriving the class from @a jlist.
   */
  explicit MutableList(Env env, jobject jlist)
    : BaseMutableList(env, jlist)
    {}

  /**
   * Constructs and wraps an empty list of type @c java.util.ArrayList
   * with initial allocation size @a length.
   */
  explicit MutableList(Env env, jint length = 0)
    : BaseMutableList(env, length)
    {}

  /**
   * Appends @a obj to the end of the list.
   */
  void add(const T& obj)
    {
      BaseMutableList::add(obj.get());
    }

  /**
   * Returns a wrapper object for the object reference at @a index.
   */
  T operator[](jint index) const
    {
      return T(m_env, BaseMutableList::operator[](index));
    }
};

} // namespace Java

#endif // SVN_JAVAHL_JNIWRAPPER_LIST_HPP
