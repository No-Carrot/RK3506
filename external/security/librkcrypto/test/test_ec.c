/*
 * Copyright (c) 20224 Rockchip Electronics Co. Ltd.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rkcrypto_core.h"
#include "rkcrypto_trace.h"
#include "cmode_adapter.h"
#include "test_ec.h"
#include "test_utils.h"
#include "ec_key_data.h"
struct test_ec_item {
	enum RK_EC_CURVE	curve;
	const char		*curve_name;
	uint32_t		hash_algo;
};

#define TEST_EC_VERIFY(name, hash)	{RK_EC_CURVE_##name, #name, hash}
#define TEST_EC_EMPTY()			{0, NULL, 0}

static const struct test_ec_item test_ec_tbl[] = {
	TEST_EC_VERIFY(SM2,  RK_ALGO_SM3),
	TEST_EC_VERIFY(P192, RK_ALGO_SHA1),
	TEST_EC_VERIFY(P224, RK_ALGO_MD5),
	TEST_EC_VERIFY(P256, RK_ALGO_SHA256),
	TEST_EC_VERIFY(P384, RK_ALGO_SHA384),
	TEST_EC_VERIFY(P521, RK_ALGO_SHA512),
};

static RK_RES test_ec_verify(enum RK_EC_CURVE curve, const char *name,
			    uint32_t hash_algo, int verbose)
{
	RK_RES res;
	uint8_t data[128];
	uint8_t sign[256];
	uint8_t digest[SHA512_HASH_SIZE];
	uint32_t digest_len = sizeof(digest);
	uint32_t sign_len = sizeof(sign);
	rk_ec_pub_key_pack pub_key;
	rk_ec_priv_key_pack priv_key;

	memset(data, 0xab, sizeof(data));
	memset(digest, 0x00, sizeof(digest));

	res = rk_hash_virt(hash_algo, data, sizeof(data), digest, &digest_len);
	if (res) {
		printf("rk_hash_virt[%u] %x\n", hash_algo, res);
		goto exit;
	}

	test_init_ec_pubkey(&pub_key, curve);
	test_init_ec_privkey(&priv_key, curve);

	res = soft_ec_sign(&priv_key, digest, digest_len, sign, &sign_len);
	if (res) {
		printf("c_mode sign failed %x\n", res);
		goto exit;
	}

	res = rk_ec_verify(&pub_key, hash_algo, NULL, 0, digest, (uint8_t *)sign, sign_len);
	if (res) {
		if (res != RK_CRYPTO_ERR_NOT_SUPPORTED)
			printf("rk_ec_verify failed %x\n", res);
		else {
			res = RK_CRYPTO_SUCCESS;

			if (verbose)
				printf("******** %-20s \t test N/A   !!! ********\n", name);
		}
		goto exit;
	}

	/* modify sign data to make it wrong */
	*sign += 1;
	res = rk_ec_verify(&pub_key, hash_algo, NULL, 0, digest, (uint8_t *)sign, sign_len);
	if (res != RK_CRYPTO_ERR_VERIFY) {
		printf("rk_ec_verify should be RK_CRYPTO_ERR_VERIFY but %x\n", res);
		goto exit;
	}

	res = RK_CRYPTO_SUCCESS;

	if (verbose)
		printf("******** %-20s \t test PASS  !!! ********\n", name);

exit:
	if (res && verbose)
		printf("******** %-20s \t test FAIL  !!! ********\n", name);

	return res;
}

RK_RES test_ec(int verbose)
{
	RK_RES res = RK_CRYPTO_ERR_GENERIC;
	uint32_t i;

	res = rk_crypto_init();
	if (res) {
		printf("rk_crypto_init error %08x\n", res);
		return res;
	}

	for (i = 0; i < ARRAY_SIZE(test_ec_tbl); i++) {
		const struct test_ec_item *test = &test_ec_tbl[i];

		/* verbose == 0 means called by the stress test, only test ECC_P256 */
		if (!verbose && test->curve != RK_EC_CURVE_P256)
			continue;

		if (!test->curve_name) {
			if (verbose)
				printf("\n");

			continue;
		}

		test_ec_verify(test->curve, test->curve_name, test->hash_algo, verbose);
	}
	rk_crypto_deinit();
	return res;
}
