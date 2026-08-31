/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 */

#include <stdlib.h>
#include <string.h>

#include "rkcrypto_core.h"
#include "rkcrypto_core_int.h"
#include "rkcrypto_ec_helper.h"
#include "rkcrypto_trace.h"

uint32_t rk_ec_get_curve_bits(uint32_t curve)
{
	uint32_t nbits = 0;
	switch (curve) {
	case RK_EC_CURVE_SM2:
		nbits = 256;
		break;
	case RK_EC_CURVE_P192:
		nbits = 192;
		break;
	case RK_EC_CURVE_P224:
		nbits = 224;
		break;
	case RK_EC_CURVE_P256:
		nbits = 256;
		break;
	case RK_EC_CURVE_P384:
		nbits = 384;
		break;
	case RK_EC_CURVE_P521:
		nbits = 521;
		break;
	default:
		nbits = 0;
	}

	return nbits;
}

RK_RES rk_ec_pubkey_encode(rk_ec_pub_key_pack *pub,
			  uint8_t *asn1_key, uint16_t *asn1_key_len, uint16_t *key_bits)
{
	RK_RES res = RK_CRYPTO_ERR_GENERIC;
	uint16_t out_max;
	rk_ec_pub_key *ec_key = &pub->key;
	uint8_t offset = 0;
	uint32_t nbits = rk_ec_get_curve_bits(ec_key->curve);
	uint32_t nbytes = ROUNDUP(nbits, 8) / 8;

	RK_CRYPTO_CHECK_PARAM(nbits == 0);
	RK_CRYPTO_CHECK_PARAM(!asn1_key || !asn1_key_len || !pub || !key_bits);
	RK_CRYPTO_CHECK_PARAM(!ec_key->x || ec_key->x_len == 0);
	RK_CRYPTO_CHECK_PARAM(!ec_key->y || ec_key->y_len == 0);
	RK_CRYPTO_CHECK_PARAM(pub->key_type != RK_EC_KEY_TYPE_PLAIN);

	if (ec_key->curve != RK_EC_CURVE_P521) {
		RK_CRYPTO_CHECK_PARAM(ec_key->x_len != ec_key->y_len);
		RK_CRYPTO_CHECK_PARAM(ec_key->x_len != nbytes);
	} else {
		RK_CRYPTO_CHECK_PARAM(ec_key->x_len != nbytes && ec_key->x_len != nbytes - 1);
		RK_CRYPTO_CHECK_PARAM(ec_key->y_len != nbytes && ec_key->y_len != nbytes - 1);
	}

	out_max = *asn1_key_len;

	if (out_max < 1 + ec_key->x_len + ec_key->y_len) {
		D_TRACE("ec pub asn1 buffer too small!");
		goto exit;
	}

	asn1_key[0] = 0x04;

	offset++;

	memcpy(asn1_key + offset, ec_key->x, ec_key->x_len);
	offset += ec_key->x_len;

	memcpy(asn1_key + offset, ec_key->y, ec_key->y_len);
	offset += ec_key->y_len;

	*asn1_key_len = offset;
	*key_bits = nbits;

	res = RK_CRYPTO_SUCCESS;
exit:
	return res;
}

/*
 * The rk_ec_priv_key will be encoded in BER format as follows
 */
RK_RES rk_ec_privkey_encode(rk_ec_priv_key_pack *priv,
			    uint8_t *asn1_key, uint16_t *asn1_key_len, uint16_t *key_bits)
{
	RK_RES res = RK_CRYPTO_ERR_GENERIC;
	uint16_t out_max;
	rk_ec_priv_key *ec_key = &priv->key;
	uint8_t offset = 0;
	uint32_t nbits = rk_ec_get_curve_bits(ec_key->curve);
	uint32_t nbytes = ROUNDUP(nbits, 8) / 8;

	RK_CRYPTO_CHECK_PARAM(!asn1_key || !asn1_key_len || !priv || !key_bits);
	RK_CRYPTO_CHECK_PARAM(priv->key_type != RK_EC_KEY_TYPE_PLAIN);
	RK_CRYPTO_CHECK_PARAM(!ec_key->d || ec_key->d_len == 0);
	RK_CRYPTO_CHECK_PARAM(ec_key->d_len != nbytes);

	out_max = *asn1_key_len;

	if (out_max < ec_key->d_len) {
		D_TRACE("ec priv asn1 buffer too small!");
		goto exit;
	}

	memcpy(asn1_key, ec_key->d, ec_key->d_len);

	*asn1_key_len = offset;
	*key_bits = nbits;

	res = RK_CRYPTO_SUCCESS;
exit:
	return res;
}