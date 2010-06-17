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

package org.apache.subversion.javahl;

import java.util.Set;
import java.io.OutputStream;
import java.io.InputStream;
import java.io.File;

import org.apache.subversion.javahl.callback.ReposNotifyCallback;

/**
 * This class offers the same commands as the svnadmin commandline
 * client.
 */
public class SVNAdmin implements ISVNAdmin
{
    /**
     * Load the required native library.
     */
    static
    {
        NativeResources.loadNativeLibrary();
    }

    /**
     * Standard empty contructor, builds just the native peer.
     */
    public SVNAdmin()
    {
        cppAddr = ctNative();
    }

    /**
     * Don't call this function!  public for backward compat purposes.
     */
    public long getCppAddr()
    {
        return cppAddr;
    }

    /**
     * Build the native peer
     * @return the adress of the peer
     */
    private native long ctNative();

    /**
     * release the native peer (should not depend on finalize)
     */
    public native void dispose();

    /**
     * release the native peer (should use dispose instead)
     * public for backward compat.  Shouldn't call this directly.
     */
    public native void finalize();

    /**
     * slot for the adress of the native peer. The JNI code is the only user
     * of this member
     */
    protected long cppAddr;

    /**
     * @return Version information about the underlying native libraries.
     */
    public Version getVersion()
    {
        return NativeResources.getVersion();
    }

    /**
     * create a subversion repository.
     * @param path                  the path where the repository will been
     *                              created.
     * @param disableFsyncCommit    disable to fsync at the commit (BDB).
     * @param keepLog               keep the log files (BDB).
     * @param configPath            optional path for user configuration files.
     * @param fstype                the type of the filesystem (BDB or FSFS)
     * @throws ClientException  throw in case of problem
     */
    public native void create(File path, boolean disableFsyncCommit,
                              boolean keepLog, File configPath,
                              String fstype) throws ClientException;

    /**
     * deltify the revisions in the repository
     * @param path              the path to the repository
     * @param start             start revision
     * @param end               end revision
     * @throws ClientException  throw in case of problem
     */
    public native void deltify(File path, Revision start, Revision end)
            throws ClientException;

    public native void dump(File path, OutputStream dataOut,
                            Revision start, Revision end, boolean incremental,
                            boolean useDeltas, ReposNotifyCallback callback)
            throws ClientException;

    /**
     * make a hot copy of the repository
     * @param path              the path to the source repository
     * @param targetPath        the path to the target repository
     * @param cleanLogs         clean the unused log files in the source
     *                          repository
     * @throws ClientException  throw in case of problem
     */
    public native void hotcopy(File path, File targetPath,
                               boolean cleanLogs) throws ClientException;

    /**
     * list all logfiles (BDB) in use or not)
     * @param path              the path to the repository
     * @param receiver          interface to receive the logfile names
     * @throws ClientException  throw in case of problem
     */
    public native void listDBLogs(File path, MessageReceiver receiver)
            throws ClientException;

    /**
     * list unused logfiles
     * @param path              the path to the repository
     * @param receiver          interface to receive the logfile names
     * @throws ClientException  throw in case of problem
     */
    public native void listUnusedDBLogs(File path, MessageReceiver receiver)
            throws ClientException;

    /**
     * interface to receive the messages
     */
    public static interface MessageReceiver
    {
        /**
         * receive one message line
         * @param message   one line of message
         */
        public void receiveMessageLine(String message);
    }

    public native void load(File path, InputStream dataInput,
                            boolean ignoreUUID, boolean forceUUID,
                            boolean usePreCommitHook, boolean usePostCommitHook,
                            String relativePath, ReposNotifyCallback callback)
            throws ClientException;

    /**
     * list all open transactions in a repository
     * @param path              the path to the repository
     * @param receiver          receives one transaction name per call
     * @throws ClientException  throw in case of problem
     */
    public native void lstxns(File path, MessageReceiver receiver)
            throws ClientException;

    public native long recover(File path, ReposNotifyCallback callback)
            throws ClientException;

    /**
     * remove open transaction in a repository
     * @param path              the path to the repository
     * @param transactions      the transactions to be removed
     * @throws ClientException  throw in case of problem
     */
    public native void rmtxns(File path, String[] transactions)
            throws ClientException;

    /**
     * Change the value of the revision property <code>propName</code>
     * to <code>propValue</code>.  By default, does not run
     * pre-/post-revprop-change hook scripts.
     *
     * @param path The path to the repository.
     * @param rev The revision for which to change a property value.
     * @param propName The name of the property to change.
     * @param propValue The new value to set for the property.
     * @param usePreRevPropChangeHook Whether to run the
     * <i>pre-revprop-change</i> hook script.
     * @param usePostRevPropChangeHook Whether to run the
     * <i>post-revprop-change</i> hook script.
     * @throws SubversionException If a problem occurs.
     * @since 1.5.0
     */
    public native void setRevProp(File path, Revision rev,
                                  String propName, String propValue,
                                  boolean usePreRevPropChangeHook,
                                  boolean usePostRevPropChangeHook)
            throws SubversionException;

    public native void verify(File path, Revision start, Revision end,
                              ReposNotifyCallback callback)
            throws ClientException;

    /**
     * list all locks in the repository
     * @param path              the path to the repository
     * @throws ClientException  throw in case of problem
     * @since 1.2
     */
    public native Set<Lock> lslocks(File path) throws ClientException;

    /**
     * remove multiple locks from the repository
     * @param path              the path to the repository
     * @param locks             the name of the locked items
     * @throws ClientException  throw in case of problem
     * @since 1.2
     */
    public native void rmlocks(File path, String[] locks)
            throws ClientException;

    public native void upgrade(File path, ReposNotifyCallback callback);
}
