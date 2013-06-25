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
import java.util.List;

/**
 * This exception is thrown whenever something goes wrong in the
 * Subversion JavaHL binding's JNI interface.
 */
public class ClientException extends NativeException
{
    // Update the serialVersionUID when there is a incompatible change
    // made to this class.  See any of the following, depending upon
    // the Java release.
    // http://java.sun.com/j2se/1.3/docs/guide/serialization/spec/version.doc7.html
    // http://java.sun.com/j2se/1.4/pdf/serial-spec.pdf
    // http://java.sun.com/j2se/1.5.0/docs/guide/serialization/spec/version.html#6678
    // http://java.sun.com/javase/6/docs/platform/serialization/spec/version.html#6678
    private static final long serialVersionUID = 2L;

    /**
     * Describes a single error message in a stack of messages
     * associated with this exception.
     * @since 1.9
     */
    public static final class ErrorMessage
    {
        ErrorMessage(int code, String message, boolean generic)
        {
            this.code = code;
            this.message = message;
            this.generic = generic;
        }

        /** @return The APR error code associated with the message. */
        public final int getCode() { return code; }

        /** @return The error message text. */
        public final String getMessage() { return message; }

        /** @return A flag indicating whether this is a generic
            message for the APR error code, or a more specific message
            generated by the native libraries. */
        public final boolean isGeneric() { return generic; }

        private final int code;
        private final String message;
        private final boolean generic;
    };

    /**
     * This constructor is only used by the native library.
     *
     * @param message A description of the problem.
     * @param source The error's source.
     * @param aprError Any associated APR error code for a wrapped
     *        <code>svn_error_t</code>.
     * @param messageStack The whole stack of error messages
     * @since 1.9
     */
    ClientException(String message, String source, int aprError,
                    List<ErrorMessage> messageStack)
    {
        super(message, source, aprError);
        this.messageStack = messageStack;
    }

    /**
     * This constructor is only used by the native library.
     *
     * @param message A description of the problem.
     * @param source The error's source.
     * @param aprError Any associated APR error code for a wrapped
     * <code>svn_error_t</code>.
     */
    ClientException(String message, String source, int aprError)
    {
        this(message, source, aprError, null);
    }

    public List<ErrorMessage> getAllMessages()
    {
        return messageStack;
    }

    /**
     * A conversion routine for maintaining backwards compatibility.
     * @param t The exception to (potentially) convert.
     * @return <code>t</code> coerced or converted into a
     * <code>ClientException</code>.
     */
    static public ClientException fromException(Throwable t)
    {
        if (t instanceof ClientException)
        {
            return (ClientException) t;
        }
        else
        {
            return new ClientException(t.getMessage(), null, -1);
        }
    }

    private final List<ErrorMessage> messageStack;
}
