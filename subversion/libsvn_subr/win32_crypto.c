/*
 * win32_crypto.c: win32 providers for SVN_AUTH_*
 *
 * ====================================================================
 * Copyright (c) 2003-2006, 2008 CollabNet.  All rights reserved.
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

#if defined(WIN32) && !defined(__MINGW32__)

/*** Includes. ***/

#include <apr_pools.h>
#include "svn_auth.h"
#include "svn_error.h"
#include "svn_utf.h"
#include "svn_config.h"
#include "svn_user.h"

#include "private/svn_auth_private.h"

#include "svn_private_config.h"

#include <wincrypt.h>
#include <apr_base64.h>

/*-----------------------------------------------------------------------*/
/* Windows simple provider, encrypts the password on Win2k and later.    */
/*-----------------------------------------------------------------------*/

/* The description string that's combined with unencrypted data by the
   Windows CryptoAPI. Used during decryption to verify that the
   encrypted data were valid. */
static const WCHAR description[] = L"auth_svn.simple.wincrypt";

/* Dynamically load the address of function NAME in PDLL into
   PFN. Return TRUE if the function name was found, otherwise
   FALSE. Equivalent to dlsym(). */
static svn_boolean_t
get_crypto_function(const char *name, HINSTANCE *pdll, FARPROC *pfn)
{
  /* In case anyone wonders why we use LoadLibraryA here: This will
     always work on Win9x/Me, whilst LoadLibraryW may not. */
  HINSTANCE dll = LoadLibraryA("Crypt32.dll");
  if (dll)
    {
      FARPROC fn = GetProcAddress(dll, name);
      if (fn)
        {
          *pdll = dll;
          *pfn = fn;
          return TRUE;
        }
      FreeLibrary(dll);
    }
  return FALSE;
}

/* Implementation of svn_auth__password_set_t that encrypts
   the incoming password using the Windows CryptoAPI. */
static svn_boolean_t
windows_password_encrypter(apr_hash_t *creds,
                           const char *realmstring,
                           const char *username,
                           const char *in,
                           svn_boolean_t non_interactive,
                           apr_pool_t *pool)
{
  typedef BOOL (CALLBACK *encrypt_fn_t)
    (DATA_BLOB *,                /* pDataIn */
     LPCWSTR,                    /* szDataDescr */
     DATA_BLOB *,                /* pOptionalEntropy */
     PVOID,                      /* pvReserved */
     CRYPTPROTECT_PROMPTSTRUCT*, /* pPromptStruct */
     DWORD,                      /* dwFlags */
     DATA_BLOB*);                /* pDataOut */

  HINSTANCE dll;
  FARPROC fn;
  encrypt_fn_t encrypt;
  DATA_BLOB blobin;
  DATA_BLOB blobout;
  svn_boolean_t crypted;

  if (!get_crypto_function("CryptProtectData", &dll, &fn))
    return FALSE;
  encrypt = (encrypt_fn_t) fn;

  blobin.cbData = strlen(in);
  blobin.pbData = (BYTE*) in;
  crypted = encrypt(&blobin, description, NULL, NULL, NULL,
                    CRYPTPROTECT_UI_FORBIDDEN, &blobout);
  if (crypted)
    {
      char *coded = apr_palloc(pool, apr_base64_encode_len(blobout.cbData));
      apr_base64_encode(coded, blobout.pbData, blobout.cbData);
      crypted = svn_auth__simple_password_set(creds, realmstring, username, 
                                              coded, non_interactive, pool);
      LocalFree(blobout.pbData);
    }

  FreeLibrary(dll);
  return crypted;
}

/* Implementation of svn_auth__password_get_t that decrypts
   the incoming password using the Windows CryptoAPI and verifies its
   validity. */
static svn_boolean_t
windows_password_decrypter(const char **out,
                           apr_hash_t *creds,
                           const char *realmstring,
                           const char *username,
                           svn_boolean_t non_interactive,
                           apr_pool_t *pool)
{
  typedef BOOL (CALLBACK * decrypt_fn_t)
    (DATA_BLOB *,                /* pDataIn */
     LPWSTR *,                   /* ppszDataDescr */
     DATA_BLOB *,                /* pOptionalEntropy */
     PVOID,                      /* pvReserved */
     CRYPTPROTECT_PROMPTSTRUCT*, /* pPromptStruct */
     DWORD,                      /* dwFlags */
     DATA_BLOB*);                /* pDataOut */

  HINSTANCE dll;
  FARPROC fn;
  DATA_BLOB blobin;
  DATA_BLOB blobout;
  LPWSTR descr;
  decrypt_fn_t decrypt;
  svn_boolean_t decrypted;
  char *in;

  if (!svn_auth__simple_password_get(&in, creds, realmstring, username,
                                     non_interactive, pool))
    return FALSE;

  if (!get_crypto_function("CryptUnprotectData", &dll, &fn))
    return FALSE;
  decrypt = (decrypt_fn_t) fn;

  blobin.cbData = strlen(in);
  blobin.pbData = apr_palloc(pool, apr_base64_decode_len(in));
  apr_base64_decode(blobin.pbData, in);
  decrypted = decrypt(&blobin, &descr, NULL, NULL, NULL,
                      CRYPTPROTECT_UI_FORBIDDEN, &blobout);
  if (decrypted)
    {
      if (0 == lstrcmpW(descr, description))
        *out = apr_pstrndup(pool, blobout.pbData, blobout.cbData);
      else
        decrypted = FALSE;
      LocalFree(blobout.pbData);
    }

  FreeLibrary(dll);
  return decrypted;
}

/* Get cached encrypted credentials from the simple provider's cache. */
static svn_error_t *
windows_simple_first_creds(void **credentials,
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
                                             windows_password_decrypter,
                                             SVN_AUTH__WINCRYPT_PASSWORD_TYPE,
                                             pool);
}

/* Save encrypted credentials to the simple provider's cache. */
static svn_error_t *
windows_simple_save_creds(svn_boolean_t *saved,
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
                                            windows_password_encrypter,
                                            SVN_AUTH__WINCRYPT_PASSWORD_TYPE,
                                            pool);
}

static const svn_auth_provider_t windows_simple_provider = {
  SVN_AUTH_CRED_SIMPLE,
  windows_simple_first_creds,
  NULL,
  windows_simple_save_creds
};


/* Public API */
void
svn_auth_get_windows_simple_provider(svn_auth_provider_object_t **provider,
                                     apr_pool_t *pool)
{
  svn_auth_provider_object_t *po = apr_pcalloc(pool, sizeof(*po));

  po->vtable = &windows_simple_provider;
  *provider = po;
}


/*-----------------------------------------------------------------------*/
/* Windows SSL server trust provider, validates ssl certificate using    */
/* CryptoApi.                                                            */
/*-----------------------------------------------------------------------*/

typedef PCCERT_CONTEXT (WINAPI *createcertcontext_fn_t)(
    DWORD dwCertEncodingType,
    const BYTE *pbCertEncoded,
    DWORD cbCertEncoded);

typedef BOOL (WINAPI *getcertchain_fn_t)(
  HCERTCHAINENGINE hChainEngine,
  PCCERT_CONTEXT pCertContext,
  LPFILETIME pTime,
  HCERTSTORE hAdditionalStore,
  PCERT_CHAIN_PARA pChainPara,
  DWORD dwFlags,
  LPVOID pvReserved,
  PCCERT_CHAIN_CONTEXT* ppChainContext);

typedef VOID (WINAPI *freecertchain_fn_t)(
  PCCERT_CHAIN_CONTEXT pChainContext);

typedef BOOL (WINAPI *freecertcontext_fn_t)(
  PCCERT_CONTEXT pCertContext);

typedef struct {
  HINSTANCE cryptodll;
  createcertcontext_fn_t createcertcontext;
  getcertchain_fn_t getcertchain;
  freecertchain_fn_t freecertchain;
  freecertcontext_fn_t freecertcontext;
} windows_ssl_server_trust_provider_baton_t;

/* Retrieve ssl server CA failure overrides (if any) from CryptoApi. */
static svn_error_t *
windows_ssl_server_trust_first_credentials(void **credentials,
                                           void **iter_baton,
                                           void *provider_baton,
                                           apr_hash_t *parameters,
                                           const char *realmstring,
                                           apr_pool_t *pool)
{
  PCCERT_CONTEXT cert_context = NULL;
  CERT_CHAIN_PARA chain_para;
  PCCERT_CHAIN_CONTEXT chain_context = NULL;
  svn_boolean_t ok = TRUE;
  windows_ssl_server_trust_provider_baton_t *pb = provider_baton;

  apr_uint32_t *failures = apr_hash_get(parameters,
                                        SVN_AUTH_PARAM_SSL_SERVER_FAILURES,
                                        APR_HASH_KEY_STRING);
  const svn_auth_ssl_server_cert_info_t *cert_info =
    apr_hash_get(parameters,
                 SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO,
                 APR_HASH_KEY_STRING);

  if (*failures & ~SVN_AUTH_SSL_UNKNOWNCA)
    {
      /* give up, go on to next provider; the only thing we can accept
         is an unknown certificate authority. */

      *credentials = NULL;
      return SVN_NO_ERROR;
    }

  if (!pb->cryptodll)
    {
      /* give up, go on to next provider. */
      *credentials = NULL;
      return SVN_NO_ERROR;
    }

  if (!pb->createcertcontext || !pb->getcertchain || !pb->freecertchain
      || !pb->freecertcontext)
    ok = FALSE;

  if (ok)
    {
      int cert_len;
      char *binary_cert;

      /* Use apr-util as CryptStringToBinaryA is available only on XP+. */
      binary_cert = apr_palloc(pool,
                               apr_base64_decode_len(cert_info->ascii_cert));
      cert_len = apr_base64_decode(binary_cert, cert_info->ascii_cert);

      /* Parse the certificate into a context. */
      cert_context = pb->createcertcontext
        (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, binary_cert, cert_len);

      if (!cert_context)
        ok = FALSE; /* Windows does not think the certificate is valid. */
    }

  if (ok)
    {
       /* Retrieve the certificate chain of the certificate
          (a certificate without a valid root does not have a chain). */
       memset(&chain_para, 0, sizeof(chain_para));
       chain_para.cbSize = sizeof(chain_para);

       if (pb->getcertchain(NULL, cert_context, NULL, NULL, &chain_para,
                        CERT_CHAIN_CACHE_END_CERT,
                        NULL, &chain_context))
         {
           if (chain_context->rgpChain[0]->TrustStatus.dwErrorStatus
               != CERT_TRUST_NO_ERROR)
            {
              /* The certificate is not 100% valid, just fall back to the
                 Subversion certificate handling. */

              ok = FALSE;
            }
         }
       else
         ok = FALSE;
    }

  if (chain_context)
    pb->freecertchain(chain_context);
  if (cert_context)
    pb->freecertcontext(cert_context);

  if (!ok)
    {
      /* go on to next provider. */
      *credentials = NULL;
      return SVN_NO_ERROR;
    }
  else
    {
      svn_auth_cred_ssl_server_trust_t *creds =
        apr_pcalloc(pool, sizeof(*creds));
      creds->may_save = FALSE; /* No need to save it. */
      *credentials = creds;
    }

  return SVN_NO_ERROR;
}

static apr_status_t
windows_ssl_server_trust_cleanup(void *baton)
{
  windows_ssl_server_trust_provider_baton_t *pb = baton;
  if (pb->cryptodll)
    {
      FreeLibrary(pb->cryptodll);
      pb->cryptodll = NULL;
      pb->createcertcontext = NULL;
      pb->freecertchain = NULL;
      pb->freecertcontext = NULL;
      pb->getcertchain = NULL;
    }
  return APR_SUCCESS;
}

static const svn_auth_provider_t windows_server_trust_provider = {
  SVN_AUTH_CRED_SSL_SERVER_TRUST,
  windows_ssl_server_trust_first_credentials,
  NULL,
  NULL,
};

/* Public API */
void
svn_auth_get_windows_ssl_server_trust_provider
  (svn_auth_provider_object_t **provider, apr_pool_t *pool)
{
  svn_auth_provider_object_t *po = apr_pcalloc(pool, sizeof(*po));
  windows_ssl_server_trust_provider_baton_t *pb =
    apr_pcalloc(pool, sizeof(*pb));

  /* In case anyone wonders why we use LoadLibraryA here: This will
     always work on Win9x/Me, whilst LoadLibraryW may not. */
  pb->cryptodll = LoadLibraryA("Crypt32.dll");
  if (pb->cryptodll)
    {
      pb->createcertcontext =
        (createcertcontext_fn_t)GetProcAddress(pb->cryptodll,
                                               "CertCreateCertificateContext");
      pb->getcertchain =
        (getcertchain_fn_t)GetProcAddress(pb->cryptodll,
                                          "CertGetCertificateChain");
      pb->freecertchain =
        (freecertchain_fn_t)GetProcAddress(pb->cryptodll,
                                           "CertFreeCertificateChain");
      pb->freecertcontext =
        (freecertcontext_fn_t)GetProcAddress(pb->cryptodll,
                                             "CertFreeCertificateContext");
      apr_pool_cleanup_register(pool, pb, windows_ssl_server_trust_cleanup,
                                apr_pool_cleanup_null);
    }

  po->vtable = &windows_server_trust_provider;
  po->provider_baton = pb;
  *provider = po;
}

#endif /* WIN32 */
