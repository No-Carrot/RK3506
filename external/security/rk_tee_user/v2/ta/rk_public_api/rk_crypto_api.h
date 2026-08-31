/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 Rockchip Electronics Co. Ltd.
 */

#ifndef RK_CRYPTO_API_H
#define RK_CRYPTO_API_H

#include <stdint.h>

#define DES_BLOCK_SIZE		8
#define AES_BLOCK_SIZE		16
#define SM4_BLOCK_SIZE		16
#define SHA1_HASH_SIZE		20
#define SHA224_HASH_SIZE	28
#define SHA256_HASH_SIZE	32
#define SHA384_HASH_SIZE	48
#define SHA512_HASH_SIZE	64
#define MD5_HASH_SIZE		16
#define SM3_HASH_SIZE		32
#define AES_AE_DATA_BLOCK	128
#define MAX_HASH_BLOCK_SIZE	128 /* SHA-512 is the longest so far */

typedef enum {
	PKCS7_PADDING = 0,
	NO_PADDING  = 1,
} rk_padding_t;

typedef enum {
	SIGN_DIGEST = 0,
	SIGN_DATA  = 1,
} rk_sign_type_t;

typedef enum {
	RK_MAC_SIGN = 3,
	RK_MAC_VERIFY = 13,
} rk_mac_mode_t;

typedef enum {
	RK_AE_UPDATE_DATA = 0,
	RK_AE_UPDATE_AAD = 1,
} rk_ae_update_type_t;

typedef struct crypto_context {
	TEE_OperationHandle op;
	TEE_ObjectHandle obj;
	TEE_ObjectHandle obj_2; //for those one operation with two keys, such as aes-xts
	TEE_OperationMode mode;
	uint32_t algo;
	uint32_t padding;
	uint32_t sign_mode;
	uint8_t buffer[16];
	uint32_t buffer_offs;
} crypto_ctx_t;

/*
 * ecc key max size is 521 bits, so 66 bytes is enough.
 * set buffer to 68 bytes for 4 byte alignment.
 */
typedef struct EC_KEY {
	uint32_t d_len;
	uint32_t x_len;
	uint32_t y_len;
	uint32_t curve;
	uint32_t key_len;
	uint8_t d[68];
	uint8_t x[68];
	uint8_t y[68];
} ec_key_t;

typedef struct EC_PUB_KEY {
	uint32_t x_len;
	uint32_t y_len;
	uint32_t curve;
	uint32_t key_len;
	uint8_t x[68];
	uint8_t y[68];
} ec_pub_key_t;

typedef struct RSA_KEY {
	uint16_t key_len;
	uint16_t d_len;
	uint16_t e_len;
	uint16_t p_len;
	uint16_t q_len;
	uint16_t dp_len;
	uint16_t dq_len;
	uint16_t iq_len;
	uint8_t n[512];
	uint8_t e[8];
	uint8_t d[512];
	uint8_t p[256];
	uint8_t q[256];
	uint8_t dp[256];
	uint8_t dq[256];
	uint8_t iq[256];
} rsa_key_t;

#define RK_MAX_SM2_KEP_ID_SIZE		32
typedef struct {
	ec_key_t	my_key;
	ec_key_t	my_eph_key;	/* ephemeral key */
	ec_pub_key_t	pub_B;
	ec_pub_key_t	eph_pub_B;	/* ephemeral public key */
	uint8_t		id_a[RK_MAX_SM2_KEP_ID_SIZE];
	uint32_t	id_a_len;
	uint8_t		id_b[RK_MAX_SM2_KEP_ID_SIZE];
	uint32_t	id_b_len;
	uint8_t		conf_in[SM3_HASH_SIZE];
	uint32_t	conf_in_len;
	uint32_t	is_initiator;
} rk_sm2_kep_parms;

enum RK_OEM_OTP_KEYID {
	RK_OEM_OTP_KEY0 = 0,
	RK_OEM_OTP_KEY1 = 1,
	RK_OEM_OTP_KEY2 = 2,
	RK_OEM_OTP_KEY3 = 3,
	RK_OEM_OTP_KEY_FW = 10, //keyid of fw_encryption_key
	RK_OEM_OTP_KEYMAX
};

enum RK_ESCK_KEYID {
	RK_ESCK_KEY0 = 0,
	RK_ESCK_KEYMAX
};

/* must same with enum RK_CRYPTO_ALGO */
enum RK_KEYLAD_ALGO {
	KL_ALGO_AES = 1,
	KL_ALGO_DES,
	KL_ALGO_TDES,
	KL_ALGO_SM4,
	KL_ALGO_MAX
};

/* must same with enum RK_CIPIHER_MODE  */
enum RK_KEYLAD_MODE {
	KL_MODE_ECB = 0,
	KL_MODE_CBC,
	KL_MODE_CTS,
	KL_MODE_CTR,
	KL_MODE_CFB,
	KL_MODE_OFB,
	KL_MODE_XTS,
	KL_MODE_CCM,
	KL_MODE_GCM,
	KL_MODE_CMAC,
	KL_MODE_CBC_MAC,
	KL_MODE_MAX
};

/* Algorithm operation */
#define RK_MODE_ENCRYPT			1
#define RK_MODE_DECRYPT			0

typedef struct {
	uint32_t tsp_index;
	uint32_t key_group;
	uint32_t is_odd_cw;
	uint32_t reserved;
} keylad_tsp_cfg;

typedef struct {
	uint32_t algo;
	uint32_t mode;
	uint32_t enc;
	uint32_t len;
	uint8_t buff[32];
	uint8_t iv[16];
} keylad_data_t;

typedef struct {
	keylad_data_t ek3;
	keylad_data_t ek2;
	keylad_data_t ek1;
	keylad_data_t nonce;
} keylad_flow_data;

crypto_ctx_t *rk_crypto_malloc_ctx(void);
void rk_crypto_free_ctx(crypto_ctx_t **ctx);

TEE_Result rk_hash_crypto(uint8_t *in, uint8_t *out, uint32_t in_len,
			  uint32_t out_len, uint32_t algo);
TEE_Result rk_hash_begin(crypto_ctx_t *ctx, uint32_t algo);
TEE_Result rk_hash_update(crypto_ctx_t *ctx, uint8_t *in, uint32_t in_len);
TEE_Result rk_hash_finish(crypto_ctx_t *ctx, uint8_t *in, uint8_t *out,
			  uint32_t in_len, uint32_t *out_len);

TEE_Result rk_cipher_crypto(uint8_t *in, uint8_t *out, uint32_t len,
			    uint8_t *key, uint32_t key_len, uint8_t *iv,
			    uint32_t algo, TEE_OperationMode mode);
TEE_Result rk_set_padding(crypto_ctx_t *ctx, int padding);
TEE_Result rk_cipher_begin(crypto_ctx_t *ctx, uint8_t *key, uint32_t key_len,
			   uint8_t *iv, uint32_t algo, TEE_OperationMode mode);
TEE_Result rk_cipher_update(crypto_ctx_t *ctx, uint8_t *in, uint8_t *out,
			    uint32_t in_len, uint32_t *out_len);
TEE_Result rk_cipher_finish(crypto_ctx_t *ctx, uint8_t *out, uint32_t *out_len);

TEE_Result rk_ae_begin(crypto_ctx_t *ctx, uint8_t *key, uint32_t key_len,
		       uint8_t *iv, uint32_t iv_len, uint32_t add_len, uint32_t tag_len,
		       uint32_t payload_len, uint32_t algo, TEE_OperationMode mode);
TEE_Result rk_ae_update(crypto_ctx_t *ctx, uint8_t *in, uint8_t *out,
			uint32_t in_len, uint32_t *out_len, rk_ae_update_type_t is_add);
TEE_Result rk_ae_finish(crypto_ctx_t *ctx, uint8_t *in, uint8_t *out,
			uint8_t *tag, uint32_t in_len, uint32_t *out_len, uint32_t *tag_len);

TEE_Result rk_gen_rsa_key(rsa_key_t *rsa_key, uint32_t key_len,
			  uint64_t public_exponent);
TEE_Result rk_rsa_crypto(uint8_t *in, uint8_t *out, uint32_t len,
			 rsa_key_t *key, uint32_t algo, TEE_OperationMode mode);
TEE_Result rk_rsa_sign(uint8_t *digest, uint8_t *signature, uint32_t digest_len,
		       uint32_t *signature_len, rsa_key_t *key, uint32_t salt_len,
		       uint32_t algo, TEE_OperationMode mode);
TEE_Result rk_set_sign_mode(crypto_ctx_t *ctx, unsigned int mode);
TEE_Result rk_rsa_begin(crypto_ctx_t *ctx, rsa_key_t *key, uint32_t algo,
			TEE_OperationMode mode);
TEE_Result rk_rsa_finish(crypto_ctx_t *ctx, uint8_t *in, uint8_t *out,
			 uint32_t in_len,
			 uint32_t *out_len, uint32_t salt_len);

TEE_Result rk_gen_ec_key(ec_key_t *ec_key, uint32_t key_len, uint32_t curve);
TEE_Result rk_ecdh_genkey(uint8_t *private, uint8_t *publicx, uint32_t *publicy,
			  uint32_t algo,  uint32_t curve, uint32_t keysize, uint8_t *out);
TEE_Result rk_ecdsa_sign(uint8_t *digest, uint8_t *signature,
			 uint32_t digest_len, uint32_t *signature_len,
			 ec_key_t *key, uint32_t algo, TEE_OperationMode mode);
TEE_Result rk_ecdsa_begin(crypto_ctx_t *ctx, ec_key_t *key, uint32_t algo,
			  TEE_OperationMode mode);
TEE_Result rk_ecdsa_finish(crypto_ctx_t *ctx, uint8_t *in, uint8_t *out,
			   uint32_t in_len, uint32_t *out_len);

TEE_Result rk_sm2_pke(uint8_t *in, uint32_t in_len, uint8_t *out,
		      uint32_t *out_len, ec_key_t *key, uint32_t algo,
		      TEE_OperationMode mode);
TEE_Result rk_sm2_dsa_sm3(uint8_t *digest, uint32_t digest_len,
			  uint8_t *signature, uint32_t *signature_len,
			  ec_key_t *key, uint32_t algo, TEE_OperationMode mode);
TEE_Result rk_sm2_kep_genkey(rk_sm2_kep_parms *kep_parms, uint8_t *share_key,
			     uint32_t share_key_len, uint8_t *conf_out);

TEE_Result rk_mac_crypto(uint8_t *in, uint8_t *out, uint32_t in_len,
			 uint32_t *out_len, uint8_t *key, uint32_t key_len,
			 uint8_t *iv, uint32_t algo);
TEE_Result rk_mac_begin(crypto_ctx_t *ctx, uint8_t *key, uint32_t key_len,
			uint8_t *iv, uint32_t algo);
TEE_Result rk_mac_update(crypto_ctx_t *ctx, uint8_t *in, uint32_t in_len);
TEE_Result rk_mac_finish(crypto_ctx_t *ctx, uint8_t *in, uint8_t *mac,
			 uint32_t in_len, uint32_t *mac_len, rk_mac_mode_t mode);

TEE_Result rk_hkdf_genkey(uint8_t *ikm, uint32_t ikm_len,
			  uint8_t *salt, uint32_t salt_len, uint32_t *info, uint32_t info_len,
			  uint32_t algo, uint32_t okm_len, uint8_t *okm);
TEE_Result rk_pkcs5_pbkdf2_hmac(uint8_t *password, uint32_t password_len,
				uint8_t *salt, uint32_t salt_len, uint32_t iterations,
				uint32_t algo, uint32_t key_len, uint8_t *out_key);

TEE_Result rk_keylad_hmac(uint32_t algo, enum RK_OEM_OTP_KEYID otp_keyid, uint32_t key_len,
			  uint8_t *msg, uint32_t msg_len, uint8_t *mac, uint32_t *mac_len);

TEE_Result rk_keylad_flow_normal(enum RK_OEM_OTP_KEYID keyid, keylad_tsp_cfg *tsp_cfg,
				 keylad_flow_data *flow_data, keylad_data_t *da);

TEE_Result rk_keylad_flow_derive(uint16_t vendor_id, keylad_tsp_cfg *tsp_cfg,
				 keylad_flow_data *flow_data, keylad_data_t *da);

TEE_Result rk_oem_otp_key_set_read_lock(enum RK_OEM_OTP_KEYID key_id);

TEE_Result rk_esck_set_read_lock(enum RK_ESCK_KEYID key_id);

TEE_Result rk_is_oem_otp_key_written(enum RK_OEM_OTP_KEYID key_id, bool *is_written);

TEE_Result rk_is_esck_written(enum RK_ESCK_KEYID key_id, bool *is_written);

TEE_Result rk_write_oem_otp_key(enum RK_OEM_OTP_KEYID key_id, uint8_t *key, uint32_t key_len);

TEE_Result rk_write_esck(enum RK_ESCK_KEYID key_id, uint8_t *key, uint32_t key_len);

#endif /* RK_CRYPTO_API_H */
