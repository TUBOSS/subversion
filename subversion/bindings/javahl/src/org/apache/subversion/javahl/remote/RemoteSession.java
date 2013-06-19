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

package org.apache.subversion.javahl.remote;

import org.apache.subversion.javahl.types.*;
import org.apache.subversion.javahl.callback.*;

import org.apache.subversion.javahl.ISVNRemote;
import org.apache.subversion.javahl.ISVNEditor;
import org.apache.subversion.javahl.JNIObject;
import org.apache.subversion.javahl.OperationContext;
import org.apache.subversion.javahl.ClientException;

import java.lang.ref.WeakReference;
import java.util.HashSet;
import java.util.Date;
import java.util.Map;

import static java.util.concurrent.TimeUnit.MILLISECONDS;
import static java.util.concurrent.TimeUnit.NANOSECONDS;

public class RemoteSession extends JNIObject implements ISVNRemote
{
    public void dispose()
    {
        if (editors != null)
        {
            // Deactivate all open editors
            for (WeakReference<ISVNEditor> ref : editors)
            {
                ISVNEditor ed = ref.get();
                if (ed == null)
                    continue;
                ed.dispose();
                ref.clear();
            }
        }
        nativeDispose();
    }

    public native void cancelOperation() throws ClientException;

    public native void reparent(String url) throws ClientException;

    public native String getSessionUrl() throws ClientException;

    public native String getSessionRelativePath(String url)
            throws ClientException;

    public native String getReposRelativePath(String url)
            throws ClientException;

    public native String getReposUUID() throws ClientException;

    public native String getReposRootUrl() throws ClientException;

    public native long getLatestRevision() throws ClientException;

    public long getRevisionByDate(Date date) throws ClientException
    {
        long timestamp = NANOSECONDS.convert(date.getTime(), MILLISECONDS);
        return getRevisionByTimestamp(timestamp);
    }

    public native long getRevisionByTimestamp(long timestamp)
            throws ClientException;

    public native NodeKind checkPath(String path, long revision)
            throws ClientException;

    public native Map<String, Lock> getLocks(String path, Depth depth)
            throws ClientException;

    public ISVNEditor getCommitEditor() throws ClientException
    {
        ISVNEditor ed = CommitEditor.createInstance(this);
        if (editors == null)
            editors = new HashSet<WeakReference<ISVNEditor>>();
        editors.add(new WeakReference<ISVNEditor>(ed));
        return ed;
    }

    public boolean hasCapability(Capability capability)
            throws ClientException
    {
        return nativeHasCapability(capability.toString());
    }

    @Override
    public native void finalize() throws Throwable;

    /**
     * This constructor is called from JNI to get an instance.
     */
    protected RemoteSession(long cppAddr)
    {
        super(cppAddr);
    }

    private native void nativeDispose();

    private native boolean nativeHasCapability(String capability);

    /*
     * NOTE: This field is accessed from native code for callbacks.
     */
    private RemoteSessionContext sessionContext = new RemoteSessionContext();
    private class RemoteSessionContext extends OperationContext {}

    /*
     * The set of open editors. We need this in order to dispose/abort
     * the editors when the session is disposed.
     */
    private HashSet<WeakReference<ISVNEditor>> editors;
}
