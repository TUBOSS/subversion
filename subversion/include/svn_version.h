/**
 * @copyright
 * ====================================================================
 * Copyright (c) 2000-2003 CollabNet.  All rights reserved.
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
 * @file svn_version.h
 * @brief Version information.
 */

#ifndef SVN_VERSION_H
#define SVN_VERSION_H

/* Hack to prevent the resource compiler from including
   apr_general.h.  It doesn't resolve the include paths
   correctly and blows up without this.
 */
#ifndef APR_STRINGIFY
#include <apr_general.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Symbols that define the version number.
   XXX FIXME: Modify before each snapshot, milestone or release. */

/* Version numbers: <major>.<minor>.<micro> */

/** Major version number.
 *
 * Modify when incompatible changes are made to published interfaces.
 */
#define SVN_VER_MAJOR      0

/** Minor version number.
 *
 * Modify when new functionality is added or new interfaces are
 * defined, but all changes are backward compatible.
 */
#define SVN_VER_MINOR      26

/** Patch number.
 *
 * Modify for every released patch.
 */
#define SVN_VER_MICRO      0

/** Library version number.
 *
 * Modify whenever there's an incompatible change in the library ABI.
 */
#define SVN_VER_LIBRARY    1


/** Version tag: a string describing the version.
 *
 * This tag remains "dev build" in the repository so that we can always
 * see from "svn --version" that the software has been built from the
 * repository rather than a "blessed" distribution.
 *
 * During the distribution process, we automatically replace this text
 * with something like "r1504".
 */
#define SVN_VER_TAG        "r6550"


/** Number tag: a string indicating whether this is a released version.
 *
 * This tag is used to generate a version number string to identify
 * the client and server in HTTP requests, for example. It must not
 * contain any spaces. This value remains "+" in the
 * repository.
 *
 * During the distribution process, we automatically replace this text
 * with "" to indicate a baselined version.
 */
#define SVN_VER_NUMTAG     ""


/** Revision number: The repository revision number of this release.
 *
 * This constant is used to generate the build number part of the Windows
 * file version. Its value remains 0 in the repository.
 *
 * During the distribution process, we automatically replace it with
 * what we guess to be the correct revision number.
 */
#define SVN_VER_REVISION   6550


/* Version strings composed from the above definitions. */

/** Version number */
#define SVN_VER_NUM        APR_STRINGIFY(SVN_VER_MAJOR) \
                           "." APR_STRINGIFY(SVN_VER_MINOR) \
                           "." APR_STRINGIFY(SVN_VER_MICRO)

/** Version number with tag (contains no whitespace) */
#define SVN_VER_NUMBER     SVN_VER_NUM SVN_VER_NUMTAG

/** Complete version string */
#define SVN_VERSION        SVN_VER_NUM " (" SVN_VER_TAG ")"


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_VERSION_H */
