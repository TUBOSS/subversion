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
 * @file InfoCallback.cpp
 * @brief Implementation of the class InfoCallback
 */

#include "SVNClient.h"
#include "InfoCallback.h"
#include "EnumMapper.h"
#include "JNIUtil.h"
#include "svn_time.h"
#include "svn_path.h"

/**
 * Create a InfoCallback object.  jcallback is the Java callback object.
 */
InfoCallback::InfoCallback(jobject jcallback)
{
    m_callback = jcallback;
}

InfoCallback::~InfoCallback()
{
    // The m_callback does not need to be destroyed, because it is the
    // passed in parameter to the Java SVNClient.info method.
}

svn_error_t *
InfoCallback::callback(void *baton,
                       const char *path,
                       const svn_info_t *info,
                       apr_pool_t *pool)
{
    if (baton)
        return ((InfoCallback *)baton)->singleInfo(path, info, pool);

    return SVN_NO_ERROR;
}

/**
 * Callback called for a single path.
 * @param path is the path name
 * @param info the information for the path
 * @param pool to use for memory allocation.
 */
svn_error_t *
InfoCallback::singleInfo(const char *path,
                         const svn_info_t *info,
                         apr_pool_t *pool)
{
    JNIEnv *env = JNIUtil::getEnv();

    // The method id will not change during the time this library is
    // loaded, so it can be cached.
    static jmethodID mid = 0;
    if (mid == 0)
    {
        jclass clazz = env->FindClass(JAVA_PACKAGE"/InfoCallback");
        if (JNIUtil::isJavaExceptionThrown())
            return SVN_NO_ERROR;

        mid = env->GetMethodID(clazz, "singleInfo",
            "(L"JAVA_PACKAGE"/Info2;)V");
        if (JNIUtil::isJavaExceptionThrown() || mid == 0)
            return SVN_NO_ERROR;

        env->DeleteLocalRef(clazz);
        if (JNIUtil::isJavaExceptionThrown())
            return SVN_NO_ERROR;
    }

    jobject jinfo2 = createJavaInfo2(path, info, pool);
    if (jinfo2 == NULL || JNIUtil::isJavaExceptionThrown())
        return SVN_NO_ERROR;

    env->CallVoidMethod(m_callback, mid, jinfo2);
    if (JNIUtil::isJavaExceptionThrown())
        return SVN_NO_ERROR;

    env->DeleteLocalRef(jinfo2);
    // Return SVN_NO_ERROR here regardless of an exception or not.

    return SVN_NO_ERROR;
}

jobject
InfoCallback::createJavaInfo2(const char *path, const svn_info_t *info,
                              apr_pool_t *pool)
{
    JNIEnv *env = JNIUtil::getEnv();
    jclass clazz = env->FindClass(JAVA_PACKAGE "/Info2");
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    static jmethodID mid = 0;
    if (mid == 0)
    {
        mid = env->GetMethodID(clazz, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;JILjava/lang/String;"
             "Ljava/lang/String;JJLjava/lang/String;"
             "Lorg/tigris/subversion/javahl/Lock;ZILjava/lang/String;JJJ"
             "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
             "Ljava/lang/String;Ljava/lang/String;)V");
        if (mid == 0 || JNIUtil::isJavaExceptionThrown())
            return NULL;
    }

    jstring jpath = JNIUtil::makeJString(path);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jstring jurl = JNIUtil::makeJString(info->URL);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jlong jrev = info->rev;
    jint jnodeKind = EnumMapper::mapNodeKind(info->kind);
    jstring jreposRootUrl = JNIUtil::makeJString(info->repos_root_URL);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jstring jreportUUID = JNIUtil::makeJString(info->repos_UUID);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jlong jlastChangedRev = info->last_changed_rev;
    jlong jlastChangedDate = info->last_changed_date;

    jstring jlastChangedAuthor =
        JNIUtil::makeJString(info->last_changed_author);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jobject jlock = SVNClient::createJavaLock(info->lock);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jboolean jhasWcInfo = info->has_wc_info ? JNI_TRUE:JNI_FALSE;
    jint jschedule = EnumMapper::mapScheduleKind(info->schedule);
    jstring jcopyFromUrl = JNIUtil::makeJString(info->copyfrom_url);
    jlong jcopyFromRev = info->copyfrom_rev;
    jlong jtextTime = info->text_time;
    jlong jpropTime = info->prop_time;

    jstring jchecksum = JNIUtil::makeJString(info->checksum);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jstring jconflictOld = JNIUtil::makeJString(info->conflict_old);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jstring jconflictNew = JNIUtil::makeJString(info->conflict_new);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jstring jconflictWrk = JNIUtil::makeJString(info->conflict_wrk);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jstring jprejfile = JNIUtil::makeJString(info->prejfile);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    jobject jinfo2 = env->NewObject(clazz, mid, jpath, jurl, jrev, jnodeKind,
        jreposRootUrl, jreportUUID, jlastChangedRev, jlastChangedDate,
        jlastChangedAuthor, jlock, jhasWcInfo, jschedule, jcopyFromUrl,
        jcopyFromRev, jtextTime, jpropTime, jchecksum, jconflictOld,
        jconflictNew, jconflictWrk, jprejfile);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(clazz);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jpath);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jurl);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jreposRootUrl);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jlastChangedAuthor);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jlock);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jcopyFromUrl);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jchecksum);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jconflictOld);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jconflictNew);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jconflictWrk);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    env->DeleteLocalRef(jprejfile);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    return jinfo2;
}
