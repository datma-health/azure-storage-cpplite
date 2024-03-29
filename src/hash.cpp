#include "hash.h"
#include "base64.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>
#else
#ifdef USE_OPENSSL
//#include <openssl/hmac.h>
#include "tiledb_openssl_shim.h"
#else
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#endif
#endif
#include <stdexcept>

namespace azure {  namespace storage_lite {
    std::string hash(const std::string &to_sign, const std::vector<unsigned char> &key)
    {
        unsigned int digest_length = SHA256_DIGEST_LENGTH;
        unsigned char digest[SHA256_DIGEST_LENGTH];
#ifdef _WIN32
        static const BCRYPT_ALG_HANDLE hmac_sha256_algorithm_handle = []() {
            BCRYPT_ALG_HANDLE handle;
            NTSTATUS status = BCryptOpenAlgorithmProvider(&handle, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
            if (status != 0)
            {
                throw std::runtime_error("Cannot open CNG provider");
            }
            return handle;
        }();

        DWORD hash_object_size = 0;
        DWORD output_size = 0;
        BCryptGetProperty(hmac_sha256_algorithm_handle, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hash_object_size, sizeof(DWORD), &output_size, 0);

        HANDLE hash_handle;
        std::vector<char> hash_object(hash_object_size);
        BCryptCreateHash(hmac_sha256_algorithm_handle, &hash_handle, (PUCHAR)hash_object.data(), hash_object_size, (PUCHAR)key.data(), (ULONG)key.size(), 0);
        BCryptHashData(hash_handle, (PUCHAR)to_sign.data(), (ULONG)to_sign.length(), 0);
        BCryptFinishHash(hash_handle, digest, digest_length, 0);
        BCryptDestroyHash(hash_handle);
#else
#ifdef USE_OPENSSL
        if(OpenSSL_version_num() < 0x30000000L ) {
          HMAC_CTX* ctx = HMAC_CTX_new();
          HMAC_CTX_reset(ctx);
          HMAC_Init_ex(ctx, key.data(), static_cast<int>(key.size()), EVP_sha256(), NULL);
          HMAC_Update(ctx, (const unsigned char*)to_sign.c_str(), to_sign.size());
          HMAC_Final(ctx, digest, &digest_length);
          HMAC_CTX_free(ctx); }
        else {
          EVP_MAC* mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
          EVP_MAC_CTX* m_ctx = EVP_MAC_CTX_new(mac);
          char sha256[] {"SHA256"};
          OSSL_PARAM ossl_params[2];

          ossl_params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, sha256, 0);
          ossl_params[1] = OSSL_PARAM_construct_end();
          EVP_MAC_init(m_ctx, key.data(), static_cast<int>(key.size()), ossl_params);
          EVP_MAC_update(m_ctx, (const unsigned char*)to_sign.c_str(), to_sign.size());
          EVP_MAC_final(m_ctx, digest, NULL, digest_length);
          EVP_MAC_free(mac);
          EVP_MAC_CTX_free(m_ctx);
        }
#else
        gnutls_hmac_fast(GNUTLS_MAC_SHA256, key.data(), key.size(), (const unsigned char *)to_sign.data(), to_sign.size(), digest);
#endif
#endif
        return to_base64(std::vector<unsigned char>(digest, digest + digest_length));
    }
}}  // azure::storage_lite
