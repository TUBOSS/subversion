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

#ifndef SVN_JAVAHL_AUTHN_CALLBACK_HPP
#define SVN_JAVAHL_AUTHN_CALLBACK_HPP

#include "svn_auth.h"

#include "Pool.h"

#include "jniwrapper/jni_object.hpp"
#include "jniwrapper/jni_string.hpp"

namespace JavaHL {

/**
 * Object wrapper for @c org.apache.subversion.javahl.callback.AuthnCallback.
 *
 * @since New in 1.9.
 */
class AuthnCallback : public ::Java::Object
{
public:
  /**
   * Object wrapper for @c ...AuthnCallback$AuthnResult.
   */
  class AuthnResult : public ::Java::Object
  {
  public:
    /**
     * Constructs a wrapper around @a jthis.
     * The constructor does not verify the class of the wrapped object.
     */
    explicit AuthnResult(::Java::Env env, jobject jthis)
      : ::Java::Object(env, jthis)
      {}

    bool save() const
      {
        return (0 != m_env.GetBooleanField(m_jthis, m_fid_save));
      }

    bool trust() const
      {
        return (0 != m_env.GetBooleanField(m_jthis, m_fid_trust));
      }

    jstring identity() const
      {
        return jstring(m_env.GetObjectField(m_jthis, m_fid_identity));
      }

    jstring secret() const
      {
        return jstring(m_env.GetObjectField(m_jthis, m_fid_secret));
      }

  private:
    friend class ::Java::ClassCache;
    static const char* const m_class_name;
    static void static_init(::Java::Env env);

    static ::Java::FieldID m_fid_save;
    static ::Java::FieldID m_fid_trust;
    static ::Java::FieldID m_fid_identity;
    static ::Java::FieldID m_fid_secret;
  };


  /**
   * Object wrapper for @c ...AuthnCallback$SSLServerCertFailures.
   */
  class SSLServerCertFailures : public ::Java::Object
  {
  public:
    /**
     * Creates and initializes a wrapped object;
     * @a failures is a set of flags.
     */
    explicit SSLServerCertFailures(::Java::Env env, jint failures);

  private:
    friend class ::Java::ClassCache;
    static const char* const m_class_name;
    static void static_init(::Java::Env env);

    static ::Java::MethodID m_mid_ctor;
  };


  /**
   * Object wrapper for @c ...AuthnCallback$SSLServerCertInfo.
   */
  class SSLServerCertInfo : public ::Java::Object
  {
  public:
    /**
     * Creates and initializes a wrapped object;
     */
    explicit SSLServerCertInfo(::Java::Env env,
                               const ::Java::String& hostname,
                               const ::Java::String& fingerprint,
                               const ::Java::String& validFrom,
                               const ::Java::String& validUntil,
                               const ::Java::String& issuer,
                               const ::Java::String& der);

  private:
    friend class ::Java::ClassCache;
    static const char* const m_class_name;
    static void static_init(::Java::Env env);

    static ::Java::MethodID m_mid_ctor;
  };


  /**
   * Constructs a wrapper around @a jthis.
   * The constructor does not verify the class of the wrapped object.
   */
  explicit AuthnCallback(::Java::Env env, jobject jthis)
    : ::Java::Object(env, jthis)
    {}

  /**
   * Invokes the Java method AuthnCallback.usernamePrompt().
   */
  jobject username_prompt(const ::Java::String& realm, bool may_save);


  /**
   * Invokes the Java method AuthnCallback.userPasswordPrompt().
   */
  jobject user_password_prompt(const ::Java::String& realm,
                               const ::Java::String& username,
                               bool may_save);

  /**
   * Invokes the Java method AuthnCallback.sslServerTrustPrompt().
   */
  jobject ssl_server_trust_prompt(const ::Java::String& realm,
                                  const SSLServerCertFailures& failures,
                                  const SSLServerCertInfo& info,
                                  bool may_save);

  /**
   * Invokes the Java method AuthnCallback.sslClientCertPrompt().
   */
  jobject ssl_client_cert_prompt(const ::Java::String& realm, bool may_save);

  /**
   * Invokes the Java method AuthnCallback.sslClientCertPassphrasePrompt().
   */
  jobject ssl_client_cert_passphrase_prompt(const ::Java::String&
                                            realm, bool may_save);

  /**
   * Invokes the Java method AuthnCallback.allowStorePlaintextPassword().
   */
  bool allow_store_plaintext_password(const ::Java::String& realm);

  /**
   * Invokes the Java method AuthnCallback.allowStorePlaintextPassphrase().
   */
  bool allow_store_plaintext_passphrase(const ::Java::String& realm);

private:
  friend class ::Java::ClassCache;
  static const char* const m_class_name;
  static void static_init(::Java::Env env);

  static ::Java::MethodID m_mid_username_prompt;
  static ::Java::MethodID m_mid_user_password_prompt;
  static ::Java::MethodID m_mid_ssl_server_trust_prompt;
  static ::Java::MethodID m_mid_ssl_client_cert_prompt;
  static ::Java::MethodID m_mid_ssl_client_cert_passphrase_prompt;
  static ::Java::MethodID m_mid_allow_store_plaintext_password;
  static ::Java::MethodID m_mid_allow_store_plaintext_passphrase;
};


/**
 * Object wrapper for the deprecated interface
 * @c org.apache.subversion.javahl.callback.UserPasswordCallback.
 *
 * @since New in 1.9.
 */
class UserPasswordCallback : public ::Java::Object
{
public:
  /**
   * Constructs a wrapper around @a jthis.
   * The constructor does not verify the class of the wrapped object.
   */
  explicit UserPasswordCallback(::Java::Env env, jobject jthis)
    : ::Java::Object(env, jthis)
    {}

  /**
   * Invokes the Java method UserPasswordCallback.askTrustSSLServer().
   */
  jint ask_trust_ssl_server(const ::Java::String& info,
                            bool allow_permanently);

  /**
   * Invokes the Java method UserPasswordCallback.prompt().
   */
  bool prompt(const ::Java::String& realm,
              const ::Java::String& username);

  /**
   * Invokes the Java method UserPasswordCallback.askYesNo().
   */
  bool ask_yes_no(const ::Java::String& realm,
                  const ::Java::String& question,
                  bool yes_is_default);

  /**
   * Invokes the Java method UserPasswordCallback.askQuestion().
   */
  jstring ask_question(const ::Java::String& realm,
                       const ::Java::String& question,
                       bool show_answer);

  /**
   * Invokes the Java method UserPasswordCallback.getUsername().
   */
  jstring get_username();

  /**
   * Invokes the Java method UserPasswordCallback.getPassword().
   */
  jstring get_password();

  /**
   * Invokes the Java method UserPasswordCallback.prompt().
   */
  bool prompt(const ::Java::String& realm,
              const ::Java::String& username,
              bool may_save);

  /**
   * Invokes the Java method UserPasswordCallback.askQuestion().
   */
  jstring ask_question(const ::Java::String& realm,
                       const ::Java::String& question,
                       bool show_answer, bool may_save);

  /**
   * Invokes the Java method UserPasswordCallback.userAllowedSave().
   */
  bool user_allowed_save();

private:
  friend class ::Java::ClassCache;
  static const char* const m_class_name;
  static void static_init(::Java::Env env);

  static ::Java::MethodID m_mid_ask_trust_ssl_server;
  static ::Java::MethodID m_mid_prompt_2arg;
  static ::Java::MethodID m_mid_ask_yes_no;
  static ::Java::MethodID m_mid_ask_question_3arg;
  static ::Java::MethodID m_mid_get_username;
  static ::Java::MethodID m_mid_get_password;
  static ::Java::MethodID m_mid_prompt;
  static ::Java::MethodID m_mid_ask_question;
  static ::Java::MethodID m_mid_user_allowed_save;
};

} // namespace JavaHL

#endif // SVN_JAVAHL_AUTHN_CALLBACK_HPP
