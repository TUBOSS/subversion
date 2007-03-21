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
 * @file CopySources.cpp
 * @brief Implementation of the class CopySources
 */

#include <apr_pools.h>
#include "svn_client.h"

#include "Pool.h"
#include "JNIUtil.h"
#include "JNIStringHolder.h"
#include "Revision.h"
#include "CopySources.h"

CopySources::CopySources(jobjectArray jcopySources)
{
    m_copySources = jcopySources;
}

CopySources::~CopySources()
{
    // m_copySources does not need to be destroyed, because it is a
    // parameter to the Java SVNClient.copy() method, and thus not
    // explicitly managed.
}

apr_array_header_t *
CopySources::array(Pool &pool)
{
    apr_pool_t *p = pool.pool();
    if (m_copySources == NULL)
        return apr_array_make(p, 0, sizeof(svn_client_copy_source_t *));

    JNIEnv *env = JNIUtil::getEnv();
    jint nbrSources = env->GetArrayLength(m_copySources);
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;
    jclass clazz = env->FindClass(JAVA_PACKAGE "/CopySource");
    if (JNIUtil::isJavaExceptionThrown())
        return NULL;

    apr_array_header_t *copySources =
        apr_array_make(p, nbrSources, sizeof(svn_client_copy_source_t *));
    for (int i = 0; i < nbrSources; i++)
    {
        jobject copySource = env->GetObjectArrayElement(m_copySources, i);
        if (JNIUtil::isJavaExceptionThrown())
            return NULL;
        if (env->IsInstanceOf(copySource, clazz))
        {
            svn_client_copy_source_t *src =
                (svn_client_copy_source_t *) apr_palloc(p, sizeof(*src));

            // Extract the path or URL from the copy source.
            static jmethodID getPath = 0;
            if (getPath == 0)
            {
                getPath = env->GetMethodID(clazz, "getPath",
                                           "()Ljava/lang/String;");
                if (JNIUtil::isJavaExceptionThrown() || getPath == 0)
                    return NULL;
            }
            jstring jpath = (jstring)
                env->CallObjectMethod(copySource, getPath);
            JNIStringHolder path(jpath);
            if (JNIUtil::isJavaExceptionThrown())
                return NULL;
            src->path = apr_pstrdup(p, (const char *) path);
            env->DeleteLocalRef(jpath);

            // Extract source revision from the copy source.
            static jmethodID getRevision = 0;
            if (getRevision == 0)
            {
                getRevision = env->GetMethodID(clazz, "getRevision",
                    "()L" JAVA_PACKAGE "/Revision;");
                if (JNIUtil::isJavaExceptionThrown() || getRevision == 0)
                    return NULL;
            }
            jobject jrev = env->CallObjectMethod(copySource, getRevision);
            // TODO: Default this to svn_opt_revision_undefined (or HEAD)
            Revision rev(jrev);
            src->revision = (const svn_opt_revision_t *)
                apr_palloc(p, sizeof(*src->revision));
            memcpy((void *) src->revision, rev.revision(),
                   sizeof(*src->revision));
            env->DeleteLocalRef(jrev);

            // Extract pegRevision from the copy source.
            static jmethodID getPegRevision = 0;
            if (getPegRevision == 0)
            {
                getPegRevision = env->GetMethodID(clazz, "getRevision",
                    "()L" JAVA_PACKAGE "/Revision;");
                if (JNIUtil::isJavaExceptionThrown() || getPegRevision == 0)
                    return NULL;
            }
            jobject jPegRev = env->CallObjectMethod(copySource,
                                                    getPegRevision);
            Revision pegRev(jPegRev, true);
            src->peg_revision = (const svn_opt_revision_t *)
                apr_palloc(p, sizeof(*src->peg_revision));
            memcpy((void *) src->peg_revision, pegRev.revision(),
                   sizeof(*src->peg_revision));
            env->DeleteLocalRef(jPegRev);

            APR_ARRAY_PUSH(copySources, svn_client_copy_source_t *) = src;
        }
        env->DeleteLocalRef(copySource);
    }

    env->DeleteLocalRef(clazz);

    return copySources;
}
