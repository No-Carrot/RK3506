/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 */

#include <stdlib.h>
#include <string.h>

#include "rkcrypto_trace.h"
#include "rkcrypto_asn1.h"

static const uint8_t sha1_oid[]   = {0x2b, 0x0e, 0x03, 0x02, 0x1a};
static const uint8_t sha224_oid[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04};
static const uint8_t sha256_oid[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
static const uint8_t sha384_oid[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02};
static const uint8_t sha512_oid[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03};

struct hash_oid_item hash_oid_tbl[] = {
	{RK_ALGO_SHA1,   sha1_oid,   sizeof(sha1_oid)},
	{RK_ALGO_SHA224, sha224_oid, sizeof(sha224_oid)},
	{RK_ALGO_SHA256, sha256_oid, sizeof(sha256_oid)},
	{RK_ALGO_SHA384, sha384_oid, sizeof(sha384_oid)},
	{RK_ALGO_SHA512, sha512_oid, sizeof(sha512_oid)},
};

RK_RES get_oid_by_md(uint32_t hash_algo, const uint8_t **oid, uint32_t *old_len)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(hash_oid_tbl); i++) {
		if (hash_oid_tbl[i].hash_algo == hash_algo) {
			*oid     = hash_oid_tbl[i].oid;
			*old_len = hash_oid_tbl[i].oid_size;
			return RK_CRYPTO_SUCCESS;
		}
	}

	return RK_CRYPTO_ERR_PARAMETER;
}

RK_RES asn1_compose_len(uint32_t len, uint8_t *field, uint32_t *field_len)
{
	uint8_t tmp_field[4];

	if (field == NULL || field_len == NULL)
		return RK_CRYPTO_ERR_PARAMETER;

	if (len < 0x80) {
		*field     = len;
		*field_len = 1;
	} else {
		uint32_t i, j;

		tmp_field[0] = (len >> 24) & 0xff;
		tmp_field[1] = (len >> 16) & 0xff;
		tmp_field[2] = (len >> 8) & 0xff;
		tmp_field[3] = len & 0xff;

		for (i = 0; i < sizeof(tmp_field); i++) {
			if (tmp_field[i] == 0x00)
				continue;

			for (j = 0; j < sizeof(tmp_field) - i; j++)
				field[j + 1] = tmp_field[j + i];

			break;
		}
		field[0]   = 0X80 + sizeof(tmp_field) - i;
		*field_len = sizeof(tmp_field) - i + 1;
	}

	return RK_CRYPTO_SUCCESS;
}

RK_RES asn1_set_object(const uint8_t *in, uint32_t in_len, uint8_t tag, uint8_t need_plus,
		       uint8_t *out, uint32_t out_max, uint32_t *out_len)
{
	RK_RES res;
	uint8_t *pout = out;
	uint32_t field_len;
	uint8_t tmp_field[5];

	if (in == NULL || out == NULL || out_len == NULL)
		return RK_CRYPTO_ERR_PARAMETER;

	*out_len = 0;

	//padding tag field
	if (out_max < 1) {
		res = RK_CRYPTO_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	*pout = tag;
	pout++;
	out_max--;

	//padding length field
	if (need_plus && *in >= 0x80)
		res = asn1_compose_len(in_len + 1, tmp_field, &field_len);
	else
		res = asn1_compose_len(in_len, tmp_field, &field_len);

	if (res != RK_CRYPTO_SUCCESS) {
		D_TRACE("asn1_compose_len error");
		return res;
	}

	if (out_max < field_len) {
		D_TRACE("out_max = %d, field_len = %d", out_max, field_len);
		res = RK_CRYPTO_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	memmove(pout, tmp_field, field_len);
	pout    += field_len;
	out_max -= field_len;

	//padding value field
	if (need_plus && *in >= 0x80) {
		if (out_max < 1) {
			res = RK_CRYPTO_ERR_OUT_OF_MEMORY;
			goto exit;
		}

		*pout = 0x00;
		pout++;
		out_max--;
	}

	if (out_max < in_len) {
		res = RK_CRYPTO_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	memmove(pout, in, in_len);
	pout += in_len;

	*out_len = pout-out;

exit:
	return res;
}