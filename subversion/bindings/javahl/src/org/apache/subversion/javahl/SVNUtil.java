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

import org.apache.subversion.javahl.util.*;

import java.io.OutputStream;

public class SVNUtil
{
    /**
     * Options to control the behaviour of the file diff routines.
     */
    public static class DiffOptions
    {
        /**
         * To what extent whitespace should be ignored when comparing lines.
         */
        public enum IgnoreSpace
        {
            /** Do not ignore whitespace */
            none,

            /**
             * Ignore changes in sequences of whitespace characters,
             * treating each sequence of whitespace characters as a
             * single space.
             */
            change,

            /** Ignore all whitespace characters. */
            all
        }

        /**
         * @param ignoreSpace Whether and how to ignore space differences
         *        in the files. The default is {@link IgnoreSpace#none}.
         * @param ignoreEolStyle Whether to treat all end-of-line
         *        markers the same when comparing lines.  The default
         *        is <code>false</code>.
         * @param showCFunction Whether the "@@" lines of the unified
         *        diff output should include a prefix of the nearest
         *        preceding line that starts with a character that
         *        might be the initial character of a C language
         *        identifier. The default is <code>false</code>.
         */
        public DiffOptions(IgnoreSpace ignoreSpace,
                           boolean ignoreEolStyle,
                           boolean showCFunction)
        {
            this.ignoreSpace = ignoreSpace;
            this.ignoreEolStyle = ignoreEolStyle;
            this.showCFunction = showCFunction;
        }

        public final IgnoreSpace ignoreSpace;
        public final boolean ignoreEolStyle;
        public final boolean showCFunction;
    }

    /** Style for displaying conflicts in merge output. */
    public enum ConflictDisplayStyle
    {
        /** Display modified and latest, with conflict markers. */
        modified_latest,

        /**
         * Like <code>modified_latest</code>, but with an extra effort
         * to identify common sequences between modified and latest.
         */
        resolved_modified_latest,

        /** Display modified, original, and latest, with conflict markers. */
        modified_original_latest,

        /** Just display modified, with no markers. */
        modified,

        /** Just display latest, with no markers. */
        latest,

        /**
         * Like <code>modified_original_latest</code>, but
         * <em>only<em> showing conflicts.
         */
        only_conflicts
    }

    /**
     * Given three versions of a file, base
     * (<code>originalFile</code>), incoming
     * (<code>modifiedFile</code>) and current
     * (<code>latestFile</code>, produce a merged result, possibly
     * displaying conflict markers.
     *
     * @param originalFile The base file version (common ancestor)
     * @param modifiedFile The incoming file version (modified elsewhere)
     * @param latestFile The current file version (modified locally)
     * @param diffOptions Options controlling how files are compared.
     *        May be <code>null</code>.
     * @param conflictOriginal Optional custom conflict marker for
     *        the <code>originalFile</code> contents.
     * @param conflictModified Optional custom conflict marker for
     *        the <code>modifiedFile</code> contents.
     * @param conflictLatest Optional custom conflict marker for
     *        the <code>latestFile</code> contents.
     * @param conflictSeparator Optional custom conflict separator.
     * @param conflictStyle Determines how conflicts are displayed.
     * @param resultStream The stream that receives the merged output.
     */
    public static void FileMerge(String originalFile,
                                 String modifiedFile,
                                 String latestFile,
                                 DiffOptions diffOptions,

                                 String conflictOriginal,
                                 String conflictModified,
                                 String conflictLatest,
                                 String conflictSeparator,
                                 ConflictDisplayStyle conflictStyle,

                                 OutputStream resultStream)
        throws ClientException
    {
        new DiffLib().FileMerge(originalFile, modifiedFile, latestFile,
                                diffOptions,
                                conflictOriginal, conflictModified,
                                conflictLatest, conflictSeparator,
                                conflictStyle, resultStream);
    }
}