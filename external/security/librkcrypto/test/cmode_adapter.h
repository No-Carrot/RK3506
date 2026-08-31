/*
 * Copyright (c) 2022 Rockchip Electronics Co. Ltd.
 */
#ifndef _CMODE_ADAPTER_H_
#define _CMODE_ADAPTER_H_

#include "rkcrypto_common.h"

RK_RES soft_cipher(uint32_t algo, uint32_t mode, uint32_t operation,
		   uint8_t *key, uint32_t key_len, uint8_t *iv,
		   uint8_t *in, uint32_t in_len, uint8_t *out);

RK_RES soft_ae(uint32_t algo, uint32_t mode, uint32_t operation,
	       uint8_t *key, uint32_t key_len, uint8_t *iv, uint32_t iv_len,
	       uint8_t *aad, uint32_t aad_len, uint32_t tag_len,
	       uint8_t *in, uint32_t in_len, uint8_t *out, uint8_t *tag);

RK_RES soft_hash(uint32_t algo, const uint8_t *in, uint32_t in_len,
		 uint8_t *out, uint32_t *out_len);

RK_RES soft_hmac(uint32_t algo, const uint8_t *key, uint32_t key_len,
		 const uint8_t *in,  uint32_t in_len, uint8_t *out, uint32_t *out_len);

RK_RES soft_ec_sign(rk_ec_priv_key_pack *priv_key,
		   const uint8_t *hash, uint32_t hash_len,
		   uint8_t *sig, uint32_t *sig_len);
#endif

