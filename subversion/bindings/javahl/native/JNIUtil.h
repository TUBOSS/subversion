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
 * @file JNIUtil.h
 * @brief Interface of the class JNIUtil
 */

#ifndef JNIUTIL_H
#define JNIUTIL_H

#include <list>
#include <vector>
#include "Pool.h"
struct apr_pool_t;
class JNIMutex;
class SVNBase;
#include <jni.h>
#include <fstream>
#include <apr_time.h>
#include <string>
#include <vector>
struct svn_error_t;

#include "svn_error.h"

#define JAVA_PACKAGE "org/apache/subversion/javahl"

struct svn_string_t;

/**
 * Class to hold a number of JNI related utility methods.  No Objects
 * of this class are ever created.
 */
class JNIUtil
{
 public:
  static svn_error_t *preprocessPath(const char *&path, apr_pool_t *pool);

  /**
   * Throw the Java NativeException instance named by
   * exceptionClassName.  A NativeException sub-class must supply a
   * 3-arg constructor identical to that of NativeException.  @a
   * source is any file name and line number information.
   */
  static void throwNativeException(const char *exceptionClassName,
                                   const char *msg,
                                   const char *source = NULL,
                                   int aprErr = -1);

  static void throwNullPointerException(const char *message);
  static jbyteArray makeJByteArray(const void *data, int length);
  static jbyteArray makeJByteArray(const svn_string_t *str);
  static jobject createDate(apr_time_t time);
  static apr_time_t getDate(jobject jdate);
  static void logMessage(const char *message);
  static int getLogLevel();
  static void initLogFile(int level, jstring path);
  static jstring makeJString(const char *txt);
  static JNIEnv *getEnv();

  /**
   * @return Whether any Throwable has been raised.
   */
  static bool isExceptionThrown() { return isJavaExceptionThrown(); }
  static bool isJavaExceptionThrown()
    {
      return getEnv()->ExceptionCheck();
    }

  static void handleAPRError(int error, const char *op);

  /**
   * Put @a object in the list of finalized objects queued up to be
   * deleted (by another thread) during the next operation.
   *
   * @param object The C++ peer of the finalized (Java) object.
   * @since 1.4.0
   */
  static void enqueueForDeletion(SVNBase *object);

  /**
   * @deprecated Use the more appropriately named
   * enqueueForDeletion() instead.
   */
  static void putFinalizedClient(SVNBase *cl);

  /**
   * Convert any exception that may have been thrown into a textual
   * representation.  Return @c NULL if no exception has
   * occurred. Useful for converting Java @c Exceptions into @c
   * svn_error_t's.
   */
  static const char *thrownExceptionToCString(SVN::Pool &in_pool);

  /**
   * Check if a Java exception was thrown and convert it to a
   * Subversion error, using @a errorcode as the generic error code.
   */
  static svn_error_t* checkJavaException(apr_status_t errorcode);

  /**
   * Throw a Java exception corresponding to err, and run
   * svn_error_clear() on err.
   */
  static void handleSVNError(svn_error_t *err);

  static std::string makeSVNErrorMessage(svn_error_t *err,
                                         jstring *jerror_message,
                                         jobject *jmessage_stack);

  /**
   * Create and throw a java.lang.Throwable instance.
   *
   * @param name The class name (in path form, with slashes in lieu
   * of dots) of the Throwable to create and raise.
   * @param message The message text of the Throwable.
   */
  static void raiseThrowable(const char *name, const char *message);

  /**
   * Creates and throws a JNIError.
   *
   * @param message The message text of the JNIError.
   */
  static void throwError(const char *message)
    {
      raiseThrowable(JAVA_PACKAGE"/JNIError", message);
    }

  static apr_pool_t *getPool();
  static bool JNIGlobalInit(JNIEnv *env);
  static bool JNIInit(JNIEnv *env);
  static bool initializeJNIRuntime();
  enum { noLog, errorLog, exceptionLog, entryLog } LogLevel;

  /**
   * Mutex that secures the global configuration object.
   */
  static JNIMutex *g_configMutex;

 private:
  static void wrappedHandleSVNError(svn_error_t *err);
  static void putErrorsInTrace(svn_error_t *err,
                               std::vector<jobject> &stackTrace);

  /**
   * The log level of this module.
   */
  static int g_logLevel;

  /**
   * Global master pool.  All other pool are subpools of this pool.
   */
  static apr_pool_t *g_pool;

  /**
   * List of objects finalized, where the C++ peer has not yet be
   * deleted.
   */
  static std::list<SVNBase*> g_finalizedObjects;

  /**
   * Mutex to secure the g_finalizedObjects list.
   */
  static JNIMutex *g_finalizedObjectsMutex;

  /**
   * Mutex to secure the access to the log file.
   */
  static JNIMutex *g_logMutex;

  /**
   * Flag, that an exception occurred during our initialization.
   */
  static bool g_initException;

  /**
   * Flag, that one thread is in the init code.  Cannot use mutex
   * here since apr is not initialized yet.
   */
  static bool g_inInit;

  /**
   * The JNI environment used during initialization.
   */
  static JNIEnv *g_initEnv;

  /**
   * The stream to write log messages to.
   */
  static std::ofstream g_logStream;
};

/**
 * A statement macro used for checking NULL pointers, in the style of
 * SVN_ERR().
 *
 * Evaluate @a expr.  If it equals NULL, throw an NullPointerException with
 * the value @a str, and return the @a ret_val.  Otherwise, continue.
 *
 * Note that if the enclosing function returns <tt>void</tt>, @a ret_val may
 * be blank.
 */

#define SVN_JNI_NULL_PTR_EX(expr, str, ret_val) \
  if ((expr) == NULL) {                         \
    JNIUtil::throwNullPointerException(str);    \
    return ret_val;                             \
  }

/**
 * A statement macro used for checking for errors, in the style of
 * SVN_ERR().
 *
 * Evalute @a expr.  If it yields an error, handle the JNI error, and
 * return @a ret_val.  Otherwise, continue.
 *
 * Note that if the enclosing function returns <tt>void</tt>, @a ret_val may
 * be blank.
 */

#define SVN_JNI_ERR(expr, ret_val)                      \
  do {                                                  \
    svn_error_t *svn_jni_err__temp = (expr);            \
    if (svn_jni_err__temp != SVN_NO_ERROR) {            \
      JNIUtil::handleSVNError(svn_jni_err__temp);       \
      return ret_val;                                   \
    }                                                   \
  } while (0)

/**
 * The initial capacity of a create local reference frame.
 */
#define LOCAL_FRAME_SIZE            16

/**
 * A statement macro use to pop the reference frame and return NULL
 */
#define POP_AND_RETURN(ret_val)         \
  do                                    \
    {                                   \
      env->PopLocalFrame(NULL);         \
      return ret_val;                   \
    }                                   \
  while (0)

/**
 * A statement macro use to pop the reference frame and return
 */
#define POP_AND_RETURN_NOTHING()        \
  do                                    \
    {                                   \
      env->PopLocalFrame(NULL);         \
      return;                           \
    }                                   \
  while (0)


/**
 * A useful macro.
 */
#define POP_AND_RETURN_NULL             POP_AND_RETURN(NULL)

#define CPPADDR_NULL_PTR(expr, ret_val)                 \
  do {                                                  \
    if ((expr) == NULL) {                               \
      JNIUtil::throwError(_("bad C++ this"));           \
      return ret_val;                                   \
    }                                                   \
  } while (0)

#define SVN_JNI_CATCH(statement, errorcode)             \
  do {                                                  \
    do { statement; } while(0);                         \
    SVN_ERR(JNIUtil::checkJavaException((errorcode)));  \
  } while(0)

#endif  // JNIUTIL_H
