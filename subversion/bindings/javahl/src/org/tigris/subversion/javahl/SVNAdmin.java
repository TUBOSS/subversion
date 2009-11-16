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

package org.tigris.subversion.javahl;

/**
 * This class offers the same commands as the svnadmin commandline
 * client.
 */
public class SVNAdmin
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
     */
    protected native void finalize();

    /**
     * slot for the adress of the native peer. The JNI code is the only user
     * of this member
     */
    protected long cppAddr;

    /**
     * Filesystem in a Berkeley DB
     */
    public static final String BDB = "bdb";

    /**
     * Filesystem in the filesystem
     */
    public static final String FSFS = "fsfs";

    /**
     * @return Version information about the underlying native libraries.
     */
    public Version getVersion()
    {
        return NativeResources.version;
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
    public native void create(String path, boolean disableFsyncCommit,
                              boolean keepLog, String configPath,
                              String fstype) throws ClientException;

    /**
     * deltify the revisions in the repository
     * @param path              the path to the repository
     * @param start             start revision
     * @param end               end revision
     * @throws ClientException  throw in case of problem
     */
    public native void deltify(String path, Revision start, Revision end)
            throws ClientException;

    /**
     * dump the data in a repository
     * @param path              the path to the repository
     * @param dataOut           the data will be outputed here
     * @param errorOut          the messages will be outputed here
     * @param start             the first revision to be dumped
     * @param end               the last revision to be dumped
     * @param incremental       the dump will be incremantal
     * @throws ClientException  throw in case of problem
     */
    public void dump(String path, OutputInterface dataOut,
                     OutputInterface errorOut, Revision start,
                     Revision end, boolean incremental)
            throws ClientException
    {
        dump(path, dataOut, errorOut, start, end, incremental, false);
    }

    /**
     * dump the data in a repository
     * @param path              the path to the repository
     * @param dataOut           the data will be outputed here
     * @param errorOut          the messages will be outputed here
     * @param start             the first revision to be dumped
     * @param end               the last revision to be dumped
     * @param incremental       the dump will be incremantal
     * @param useDeltas         the dump will contain deltas between nodes
     * @throws ClientException  throw in case of problem
     * @since 1.5
     */
    public native void dump(String path, OutputInterface dataOut,
                            OutputInterface errorOut, Revision start,
                            Revision end, boolean incremental,
                            boolean useDeltas)
            throws ClientException;

    /**
     * make a hot copy of the repository
     * @param path              the path to the source repository
     * @param targetPath        the path to the target repository
     * @param cleanLogs         clean the unused log files in the source
     *                          repository
     * @throws ClientException  throw in case of problem
     */
    public native void hotcopy(String path, String targetPath,
                               boolean cleanLogs) throws ClientException;

    /**
     * list all logfiles (BDB) in use or not)
     * @param path              the path to the repository
     * @param receiver          interface to receive the logfile names
     * @throws ClientException  throw in case of problem
     */
    public native void listDBLogs(String path, MessageReceiver receiver)
            throws ClientException;

    /**
     * list unused logfiles
     * @param path              the path to the repository
     * @param receiver          interface to receive the logfile names
     * @throws ClientException  throw in case of problem
     */
    public native void listUnusedDBLogs(String path, MessageReceiver receiver)
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

    /**
     * load the data of a dump into a repository,
     * @param path              the path to the repository
     * @param dataInput         the data input source
     * @param messageOutput     the target for processing messages
     * @param ignoreUUID        ignore any UUID found in the input stream
     * @param forceUUID         set the repository UUID to any found in the
     *                          stream
     * @param relativePath      the directory in the repository, where the data
     *                          in put optional.
     * @throws ClientException  throw in case of problem
     */
    public void load(String path, InputInterface dataInput,
                     OutputInterface messageOutput, boolean ignoreUUID,
                     boolean forceUUID, String relativePath)
            throws ClientException
    {
        load(path, dataInput, messageOutput, ignoreUUID, forceUUID,
             false, false, relativePath);
    }

    /**
     * load the data of a dump into a repository,
     * @param path              the path to the repository
     * @param dataInput         the data input source
     * @param messageOutput     the target for processing messages
     * @param ignoreUUID        ignore any UUID found in the input stream
     * @param forceUUID         set the repository UUID to any found in the
     *                          stream
     * @param usePreCommitHook  use the pre-commit hook when processing commits
     * @param usePostCommitHook use the post-commit hook when processing commits
     * @param relativePath      the directory in the repository, where the data
     *                          in put optional.
     * @throws ClientException  throw in case of problem
     * @since 1.5
     */
    public native void load(String path, InputInterface dataInput,
                            OutputInterface messageOutput, boolean ignoreUUID,
                            boolean forceUUID, boolean usePreCommitHook,
                            boolean usePostCommitHook, String relativePath)
            throws ClientException;

    /**
     * list all open transactions in a repository
     * @param path              the path to the repository
     * @param receiver          receives one transaction name per call
     * @throws ClientException  throw in case of problem
     */
    public native void lstxns(String path, MessageReceiver receiver)
            throws ClientException;

    /**
     * recover the berkeley db of a repository, returns youngest revision
     * @param path              the path to the repository
     * @throws ClientException  throw in case of problem
     */
    public native long recover(String path) throws ClientException;

    /**
     * remove open transaction in a repository
     * @param path              the path to the repository
     * @param transactions      the transactions to be removed
     * @throws ClientException  throw in case of problem
     */
    public native void rmtxns(String path, String [] transactions)
            throws ClientException;

    /**
     * set the log message of a revision
     * @param path              the path to the repository
     * @param rev               the revision to be changed
     * @param message           the message to be set
     * @param bypassHooks       if to bypass all repository hooks
     * @throws ClientException  throw in case of problem
     * @deprecated Use setRevProp() instead.
     */
    public native void setLog(String path, Revision rev, String message,
                              boolean bypassHooks)
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
    public native void setRevProp(String path, Revision rev,
                                  String propName, String propValue,
                                  boolean usePreRevPropChangeHook,
                                  boolean usePostRevPropChangeHook)
            throws SubversionException;

    /**
     * Verify the repository at <code>path</code> between revisions
     * <code>start</code> and <code>end</code>.
     *
     * @param path              the path to the repository
     * @param messageOut        the receiver of all messages
     * @param start             the first revision
     * @param end               the last revision
     * @throws ClientException If an error occurred.
     */
    public native void verify(String path,  OutputInterface messageOut,
                              Revision start, Revision end)
            throws ClientException;

    /**
     * list all locks in the repository
     * @param path              the path to the repository
     * @throws ClientException  throw in case of problem
     * @since 1.2
     */
    public native Lock[] lslocks(String path) throws ClientException;

    /**
     * remove multiple locks from the repository
     * @param path              the path to the repository
     * @param locks             the name of the locked items
     * @throws ClientException  throw in case of problem
     * @since 1.2
     */
    public native void rmlocks(String path, String [] locks)
            throws ClientException;
}
