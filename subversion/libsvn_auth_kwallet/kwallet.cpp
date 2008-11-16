/*
 * kwallet.cpp: KWallet provider for SVN_AUTH_CRED_*
 *
 * ====================================================================
 * Copyright (c) 2008 CollabNet.  All rights reserved.
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
 */

/* ==================================================================== */



/*** Includes. ***/

#include <string.h>

#include <apr_pools.h>
#include "svn_auth.h"
#include "svn_config.h"
#include "svn_error.h"
#include "svn_version.h"

#include "private/svn_auth_private.h"

#include "svn_private_config.h"

#include <dbus/dbus.h>
#include <QtCore/QString>

#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kcomponentdata.h>
#include <klocalizedstring.h>
#include <kwallet.h>


/*-----------------------------------------------------------------------*/
/* KWallet simple provider, puts passwords in KWallet                    */
/*-----------------------------------------------------------------------*/


static QString
get_wallet_name(apr_hash_t *parameters)
{
  svn_config_t *config =
    static_cast<svn_config_t *> (apr_hash_get(parameters,
                                              SVN_AUTH_PARAM_CONFIG_CATEGORY_CONFIG,
                                              APR_HASH_KEY_STRING));
  const char *wallet_name;
  svn_config_get(config,
                 &wallet_name,
                 SVN_CONFIG_SECTION_AUTH,
                 SVN_CONFIG_OPTION_KWALLET_WALLET,
                 "");
  if (strcmp(wallet_name, "") == 0)
    {
      return KWallet::Wallet::NetworkWallet();
    }
  else
    {
      return QString::fromUtf8(wallet_name);
    }
}

static KWallet::Wallet *
get_wallet(QString wallet_name,
           apr_hash_t *parameters)
{
  KWallet::Wallet *wallet =
    static_cast<KWallet::Wallet *> (apr_hash_get(parameters,
                                                 "kwallet-wallet",
                                                 APR_HASH_KEY_STRING));
  if (! wallet && ! apr_hash_get(parameters,
                                 "kwallet-opening-failed",
                                 APR_HASH_KEY_STRING))
    {
      wallet = KWallet::Wallet::openWallet(wallet_name,
                                           -1,
                                           KWallet::Wallet::Synchronous);
    }
  if (wallet)
    {
      apr_hash_set(parameters,
                   "kwallet-wallet",
                   APR_HASH_KEY_STRING,
                   wallet);
    }
  else
    {
      apr_hash_set(parameters,
                   "kwallet-opening-failed",
                   APR_HASH_KEY_STRING,
                   "");
    }
  return wallet;
}

/* Implementation of svn_auth__password_get_t that retrieves
   the password from KWallet. */
static svn_boolean_t
kwallet_password_get(const char **password,
                     apr_hash_t *creds,
                     const char *realmstring,
                     const char *username,
                     apr_hash_t *parameters,
                     svn_boolean_t non_interactive,
                     apr_pool_t *pool)
{
  if (non_interactive)
    {
      return FALSE;
    }

  if (! dbus_bus_get(DBUS_BUS_SESSION, NULL))
    {
      return FALSE;
    }

  KCmdLineArgs::init(1,
                     (char *[1]) { (char *) "svn" },
                     "Subversion",
                     "subversion",
                     ki18n("Subversion"),
                     SVN_VER_NUMBER,
                     ki18n("Version control system"),
                     KCmdLineArgs::CmdLineArgKDE);
  KComponentData component_data(KCmdLineArgs::aboutData());
  svn_boolean_t ret = FALSE;
  QString wallet_name = get_wallet_name(parameters);
  QString folder = QString::fromUtf8("Subversion");
  QString key =
    QString::fromUtf8(username) + "@" + QString::fromUtf8(realmstring);
  if (! KWallet::Wallet::keyDoesNotExist(wallet_name, folder, key))
    {
      KWallet::Wallet *wallet = get_wallet(wallet_name, parameters);
      if (wallet)
        {
          apr_hash_set(parameters,
                       "kwallet-initialized",
                       APR_HASH_KEY_STRING,
                       "");
          if (wallet->setFolder(folder))
            {
              QString q_password;
              if (wallet->readPassword(key, q_password) == 0);
                {
                  *password = apr_pstrmemdup(pool,
                                             q_password.toUtf8().data(),
                                             q_password.size());
                  ret = TRUE;
                }
            }
        }
    }

  return ret;
}

/* Implementation of svn_auth__password_set_t that stores
   the password in KWallet. */
static svn_boolean_t
kwallet_password_set(apr_hash_t *creds,
                     const char *realmstring,
                     const char *username,
                     const char *password,
                     apr_hash_t *parameters,
                     svn_boolean_t non_interactive,
                     apr_pool_t *pool)
{
  if (non_interactive)
    {
      return FALSE;
    }

  if (! dbus_bus_get(DBUS_BUS_SESSION, NULL))
    {
      return FALSE;
    }

  KCmdLineArgs::init(1,
                     (char *[1]) { (char *) "svn" },
                     "Subversion",
                     "subversion",
                     ki18n("Subversion"),
                     SVN_VER_NUMBER,
                     ki18n("Version control system"),
                     KCmdLineArgs::CmdLineArgKDE);
  KComponentData component_data(KCmdLineArgs::aboutData());
  svn_boolean_t ret = FALSE;
  QString q_password = QString::fromUtf8(password);
  QString wallet_name = get_wallet_name(parameters);
  QString folder = QString::fromUtf8("Subversion");
  KWallet::Wallet *wallet = get_wallet(wallet_name, parameters);
  if (wallet)
    {
      apr_hash_set(parameters,
                   "kwallet-initialized",
                   APR_HASH_KEY_STRING,
                   "");
      if (! wallet->hasFolder(folder))
        {
          wallet->createFolder(folder);
        }
      if (wallet->setFolder(folder))
        {
          QString key = QString::fromUtf8(username) + "@"
            + QString::fromUtf8(realmstring);
          if (wallet->writePassword(key, q_password) == 0)
            {
              ret = TRUE;
            }
        }
    }

  return ret;
}

static svn_error_t *
kwallet_terminate(apr_hash_t *parameters,
                  apr_pool_t *pool)
{
  if (apr_hash_get(parameters, "kwallet-initialized", APR_HASH_KEY_STRING))
    {
      QString wallet_name = get_wallet_name(parameters);
      KWallet::Wallet *wallet = get_wallet(wallet_name, parameters);
      KWallet::Wallet::disconnectApplication(wallet_name,
                                             QString::fromUtf8("Subversion"));
      delete wallet;
      apr_hash_set(parameters,
                   "kwallet-initialized",
                   APR_HASH_KEY_STRING,
                   NULL);
    }
  return SVN_NO_ERROR;
}

/* Get cached encrypted credentials from the simple provider's cache. */
static svn_error_t *
kwallet_simple_first_creds(void **credentials,
                           void **iter_baton,
                           void *provider_baton,
                           apr_hash_t *parameters,
                           const char *realmstring,
                           apr_pool_t *pool)
{
  return svn_auth__simple_first_creds_helper(credentials,
                                             iter_baton,
                                             provider_baton,
                                             parameters,
                                             realmstring,
                                             kwallet_password_get,
                                             SVN_AUTH__KWALLET_PASSWORD_TYPE,
                                             pool);
}

/* Save encrypted credentials to the simple provider's cache. */
static svn_error_t *
kwallet_simple_save_creds(svn_boolean_t *saved,
                          void *credentials,
                          void *provider_baton,
                          apr_hash_t *parameters,
                          const char *realmstring,
                          apr_pool_t *pool)
{
  return svn_auth__simple_save_creds_helper(saved, credentials,
                                            provider_baton,
                                            parameters,
                                            realmstring,
                                            kwallet_password_set,
                                            SVN_AUTH__KWALLET_PASSWORD_TYPE,
                                            pool);
}

static const svn_auth_provider_t kwallet_simple_provider = {
  SVN_AUTH_CRED_SIMPLE,
  kwallet_simple_first_creds,
  NULL,
  kwallet_simple_save_creds,
  kwallet_terminate
};

/* Public API */
extern "C" {
void
svn_auth_get_kwallet_simple_provider(svn_auth_provider_object_t **provider,
                                     apr_pool_t *pool)
{
  svn_auth_provider_object_t *po =
    static_cast<svn_auth_provider_object_t *> (apr_pcalloc(pool, sizeof(*po)));

  po->vtable = &kwallet_simple_provider;
  *provider = po;
}
}


/*-----------------------------------------------------------------------*/
/* KWallet SSL client certificate passphrase provider,                   */
/* puts passphrases in KWallet                                           */
/*-----------------------------------------------------------------------*/

/* Get cached encrypted credentials from the ssl client cert password
   provider's cache. */
static svn_error_t *
kwallet_ssl_client_cert_pw_first_creds(void **credentials,
                                       void **iter_baton,
                                       void *provider_baton,
                                       apr_hash_t *parameters,
                                       const char *realmstring,
                                       apr_pool_t *pool)
{
  return svn_auth__ssl_client_cert_pw_file_first_creds_helper
           (credentials,
            iter_baton, provider_baton,
            parameters, realmstring,
            kwallet_password_get,
            SVN_AUTH__KWALLET_PASSWORD_TYPE,
            pool);
}

/* Save encrypted credentials to the ssl client cert password provider's
   cache. */
static svn_error_t *
kwallet_ssl_client_cert_pw_save_creds(svn_boolean_t *saved,
                                      void *credentials,
                                      void *provider_baton,
                                      apr_hash_t *parameters,
                                      const char *realmstring,
                                      apr_pool_t *pool)
{
  return svn_auth__ssl_client_cert_pw_file_save_creds_helper
           (saved, credentials,
            provider_baton, parameters,
            realmstring,
            kwallet_password_set,
            SVN_AUTH__KWALLET_PASSWORD_TYPE,
            pool);
}

static const svn_auth_provider_t kwallet_ssl_client_cert_pw_provider = {
  SVN_AUTH_CRED_SSL_CLIENT_CERT_PW,
  kwallet_ssl_client_cert_pw_first_creds,
  NULL,
  kwallet_ssl_client_cert_pw_save_creds,
  kwallet_terminate
};

/* Public API */
void
svn_auth_get_kwallet_ssl_client_cert_pw_provider
    (svn_auth_provider_object_t **provider,
     apr_pool_t *pool)
{
  svn_auth_provider_object_t *po =
    static_cast<svn_auth_provider_object_t *> (apr_pcalloc(pool, sizeof(*po)));

  po->vtable = &kwallet_ssl_client_cert_pw_provider;
  *provider = po;
}
