// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 */
#include <string.h>
#include <tee_internal_api.h>
#include "rktest_handle.h"
#include "../rk_public_api/rk_crypto_api.h"

static const uint32_t g_test_oem_key_id = RK_OEM_OTP_KEY1;
static const uint32_t g_test_esck_key_id = RK_ESCK_KEY0;

/****************************************************
 * ESCK data for derive test
 ****************************************************/
static const uint8_t esck_data[] = {
	0xa0, 0x29, 0x56, 0x7f, 0x45, 0x29, 0x56, 0x16,
	0x46, 0x29, 0x56, 0x17, 0xa3, 0x29, 0x56, 0x33,
};

/****************************************************
 * OEM key data RK_OEM_OTP_KEY1 for normal test
 ****************************************************/
static const uint8_t otp_key1[] = {
	0xdf, 0x20, 0x5a, 0xb3, 0x88, 0x50, 0x9e, 0x4f,
	0x01, 0x21, 0xe7, 0xc9, 0x24, 0x00, 0x1b, 0x84,
	0x2a, 0xfb, 0x83, 0xac, 0xe4, 0x61, 0x4a, 0x94,
	0x1f, 0xf4, 0x84, 0xc3, 0x1f, 0xe5, 0x52, 0xc7,
};

static const uint8_t ek3_data[] = {
	0xa4, 0xd2, 0xe1, 0x0a, 0x80, 0x17, 0x43, 0x67,
	0x05, 0x08, 0x07, 0x3d, 0xe3, 0x46, 0x13, 0x15,
};

static const uint8_t ek2_data[] = {
	0xef, 0x5e, 0xa3, 0xaf, 0x1d, 0x3b, 0x8d, 0x18,
	0xa2, 0x30, 0x68, 0x66, 0x78, 0x68, 0x59, 0x2a,
};

static const uint8_t ek1_data[] = {
	0xb5, 0x29, 0xc1, 0xef, 0xa3, 0x91, 0xc4, 0x1c,
};

static const uint8_t nonce_data[] = {
	0x60, 0x3b, 0x4f, 0xfc, 0x56, 0x60, 0x88, 0x28,
	0xc1, 0x9a, 0x91, 0xbe, 0xae, 0xf3, 0x37, 0x65,
};

static const uint8_t da_data_normal[] = {
	0x85, 0x37, 0x06, 0x8f, 0x17, 0xd1, 0x4a, 0x31,
	0xd6, 0x91, 0x26, 0xb0, 0x21, 0x9d, 0x28, 0x8d,
};

static const uint8_t da_data_derive[] = {
	0xc1, 0x8c, 0x03, 0xf6, 0x90, 0xab, 0x79, 0x19,
	0x0d, 0xad, 0xc0, 0xcb, 0x27, 0x35, 0x30, 0x6e,
};

static const uint8_t cw_data_normal[] = {
	0x27, 0xe9, 0x0e, 0x0e, 0x2d, 0x0e, 0x59, 0xa1,
};

static const uint8_t cw_data_derive[] = {
	0xdc, 0x47, 0xbe, 0x34, 0x3a, 0x26, 0x79, 0x8e,
};

static const char *kl_algo2name[] = {
	[KL_ALGO_AES]  = "AES",
	[KL_ALGO_DES]  = "DES",
	[KL_ALGO_TDES] = "TDES",
	[KL_ALGO_SM4]  = "SM4",
};

static const char *kl_mode2name[] = {
	[KL_MODE_ECB] = "ECB",
	[KL_MODE_CBC] = "CBC",
	[KL_MODE_CTS] = "CTS",
	[KL_MODE_CTR] = "CTR",
	[KL_MODE_CFB] = "CFB",
	[KL_MODE_OFB] = "OFB",
};

#define GET_ENC_NAME(x) ((x) == RK_MODE_DECRYPT ? "DECRYPT" : "ENCRYPT")

static void rk_dump_flow_test_data(keylad_flow_data *flow_data, keylad_data_t *da, bool is_derive)
{
	const char *flow_name = is_derive ? "DERIVE" : "NORMAL";

	IMSG("========================= %s CW param =========================\n", flow_name);
	IMSG("Ek3(K2)    using \t%4s %s %3ubits %s\n",
	     kl_algo2name[flow_data->ek3.algo],
	     kl_mode2name[flow_data->ek3.mode],
	     flow_data->ek3.len * 8,
	     GET_ENC_NAME(flow_data->ek3.enc));
	IMSG("Ek2(K1)    using \t%4s %s %3ubits %s\n",
	     kl_algo2name[flow_data->ek2.algo],
	     kl_mode2name[flow_data->ek2.mode],
	     flow_data->ek2.len * 8,
	     GET_ENC_NAME(flow_data->ek2.enc));
	IMSG("Ek1(CW)    using \t%4s %s %3ubits %s\n",
	     kl_algo2name[flow_data->ek1.algo],
	     kl_mode2name[flow_data->ek1.mode],
	     flow_data->ek1.len * 8,
	     GET_ENC_NAME(flow_data->ek1.enc));

	IMSG("========================= %s DA param =========================\n", flow_name);
	IMSG("Ek2(K2)    using \t%4s %s %3ubits %s\n",
	     kl_algo2name[flow_data->ek2.algo],
	     kl_mode2name[flow_data->ek2.mode],
	     flow_data->ek2.len * 8,
	     GET_ENC_NAME(flow_data->ek2.enc));
	IMSG("Dk2(Nonce) using \t%4s %s %3ubits %s\n",
	     kl_algo2name[flow_data->nonce.algo],
	     kl_mode2name[flow_data->nonce.mode],
	     da->len * 8,
	     GET_ENC_NAME(da->enc));

	if (is_derive)
		ta_dump_hex("DERIVE_CW", cw_data_derive, sizeof(cw_data_derive));
	else
		ta_dump_hex("NORMAL_CW", cw_data_normal, sizeof(cw_data_normal));

	IMSG("============================================================\n");
}

static void rk_init_flow_test_data(keylad_flow_data *flow_data, keylad_data_t *da, bool is_derive)
{
	memset(flow_data, 0x00, sizeof(*flow_data));

	flow_data->ek3.algo   = is_derive ? KL_ALGO_TDES : KL_ALGO_AES;
	flow_data->ek3.mode   = KL_MODE_ECB;
	flow_data->ek3.enc    = RK_MODE_DECRYPT;
	flow_data->ek3.len    = sizeof(ek3_data);
	memcpy(flow_data->ek3.buff, ek3_data, sizeof(ek3_data));

	flow_data->ek2.algo   = is_derive ? KL_ALGO_TDES : KL_ALGO_AES;
	flow_data->ek2.mode   = KL_MODE_ECB;
	flow_data->ek2.enc    = RK_MODE_DECRYPT;
	flow_data->ek2.len    = sizeof(ek2_data);
	memcpy(flow_data->ek2.buff, ek2_data, sizeof(ek2_data));

	/* cw is only 8byte, must use TDES */
	flow_data->ek1.algo   = KL_ALGO_TDES;
	flow_data->ek1.mode   = KL_MODE_ECB;
	flow_data->ek1.enc    = RK_MODE_DECRYPT;
	flow_data->ek1.len    = sizeof(ek1_data);
	memcpy(flow_data->ek1.buff, ek1_data, sizeof(ek1_data));

	flow_data->nonce.algo = is_derive ? KL_ALGO_TDES : KL_ALGO_AES;
	flow_data->nonce.mode = KL_MODE_ECB;
	flow_data->nonce.enc  = RK_MODE_DECRYPT;
	flow_data->nonce.len  = sizeof(nonce_data);
	memcpy(flow_data->nonce.buff, nonce_data, sizeof(nonce_data));

	memset(da, 0x00, sizeof(*da));
	da->algo = flow_data->nonce.algo;
	da->mode = flow_data->nonce.mode;
	da->enc  = flow_data->nonce.enc;
	da->len  = sizeof(da_data_normal);
}

static TEE_Result rk_keylad_flow_normal_test(void)
{
	bool is_key_written = false;
	TEE_Result res;

	uint32_t otp_key_id = 0;
	keylad_tsp_cfg tsp_cfg;
	keylad_data_t da;
	keylad_flow_data flow_data;

	IMSG("---------------------------- %s start --------------------------\n", __func__);
	res = rk_is_oem_otp_key_written(g_test_oem_key_id, &is_key_written);
	if (res == TEE_ERROR_NOT_SUPPORTED) {
		IMSG("This platform do not support esck. Skip.");
		goto exit;
	} else if (res == TEE_ERROR_ACCESS_DENIED) {
		IMSG("ESCK%d is locked.\n", g_test_esck_key_id);
	} else if (res) {
		EMSG("Check ESCK%d is written failed.\n", g_test_esck_key_id);
		goto exit;
	}

	if (!is_key_written && res != TEE_ERROR_ACCESS_DENIED) {
		IMSG("Please write OEM KEY%u first [ rktest keylad_write ] !!!!!!!!!!!\n",
		     g_test_oem_key_id);
		res = TEE_ERROR_BAD_STATE;
		goto exit;
	}

	/* set test param */
	otp_key_id = g_test_oem_key_id;

	rk_init_flow_test_data(&flow_data, &da, false);

	memset(&tsp_cfg, 0x00, sizeof(tsp_cfg));
	tsp_cfg.tsp_index = 1;
	tsp_cfg.key_group = 1;
	tsp_cfg.is_odd_cw = 1;

	res = rk_keylad_flow_normal(otp_key_id, &tsp_cfg, &flow_data, &da);
	if (res != TEE_SUCCESS) {
		EMSG("rk_keylad_flow_normal returned 0x%x\n", res);
		goto exit;
	}

	rk_dump_flow_test_data(&flow_data, &da, false);

	if (res || da.len != sizeof(da_data_normal) || memcmp(da_data_normal, da.buff, da.len)) {
		if (res == TEE_ERROR_NOT_SUPPORTED)
			IMSG("CRYPTO_SERVICE_CMD_KEYLAD_FLOW_NORMAL not support.");
		else
			IMSG("compare da FAIL!\n");
	} else {
		IMSG("compare da PASS!\n");
	}

exit:
	IMSG("---------------------------- %s finish --------------------------\n", __func__);

	return res;
}

static TEE_Result rk_keylad_flow_derive_test(void)
{
	TEE_Result res;
	bool is_key_written = false;
	uint16_t vendor_id = 0;
	keylad_tsp_cfg tsp_cfg;
	keylad_data_t da;
	keylad_flow_data flow_data;

	IMSG("---------------------------- %s start --------------------------\n", __func__);
	res = rk_is_esck_written(g_test_esck_key_id, &is_key_written);
	if (res == TEE_ERROR_NOT_SUPPORTED) {
		IMSG("This platform do not support esck. Skip.");
		goto exit;
	} else if (res == TEE_ERROR_ACCESS_DENIED) {
		IMSG("ESCK%d is locked.\n", g_test_esck_key_id);
	} else if (res) {
		EMSG("Check ESCK%d is written failed.\n", g_test_esck_key_id);
		goto exit;
	}

	if (!is_key_written && res != TEE_ERROR_ACCESS_DENIED) {
		IMSG("Please write ESCK KEY%u first!!!!!!!!!!!\n", g_test_esck_key_id);
		res = TEE_ERROR_BAD_STATE;
		goto exit;
	}

	/* set test param */
	vendor_id = 0xbeef;

	rk_init_flow_test_data(&flow_data, &da, true);

	memset(&tsp_cfg, 0x00, sizeof(tsp_cfg));
	tsp_cfg.tsp_index = 0;
	tsp_cfg.key_group = 0;
	tsp_cfg.is_odd_cw = 0;

	res = rk_keylad_flow_derive(vendor_id, &tsp_cfg, &flow_data, &da);
	if (res != TEE_SUCCESS) {
		EMSG("rk_keylad_flow_derive returned 0x%x\n", res);
		goto exit;
	}

	rk_dump_flow_test_data(&flow_data, &da, true);

	if (res || da.len != sizeof(da_data_derive) || memcmp(da_data_derive, da.buff, da.len)) {
		if (res == TEE_ERROR_NOT_SUPPORTED)
			IMSG("CRYPTO_SERVICE_CMD_KEYLAD_FLOW_DERIVE not support.");
		else
			IMSG("compare da FAIL!\n");
	} else {
		IMSG("compare da PASS!\n");
	}

exit:
	IMSG("---------------------------- %s finish --------------------------\n", __func__);

	return res;
}

TEE_Result handle_keylad_test(void)
{
	rk_keylad_flow_normal_test();

	IMSG("\n");

	rk_keylad_flow_derive_test();

	return TEE_SUCCESS;
}

static TEE_Result rk_keylad_test_write_esck(uint32_t key_id, uint8_t *key, uint32_t key_len)
{
	TEE_Result res = TEE_SUCCESS;
	bool is_written = false;
	int index = key_id - RK_ESCK_KEY0;

	IMSG("Start write ESCK%d data to otp...\n", index);

	res = rk_is_esck_written(key_id, &is_written);
	if (res == TEE_ERROR_NOT_SUPPORTED) {
		IMSG("This platform do not support esck. Skip.");
		goto exit;
	} else if (res == TEE_ERROR_ACCESS_DENIED) {
		IMSG("ESCK%d is locked skip write.\n", index);
		goto exit;
	} else if (res) {
		EMSG("Check ESCK%d is written failed.\n", index);
		goto exit;
	}

	if (!is_written) {
		ta_dump_hex("ESCK", key, key_len);
		res = rk_write_esck(key_id, key, key_len);
		if (res) {
			EMSG("Write ESCK%d data to otp failed.\n", index);
			goto exit;
		}

		IMSG("Write ESCK%d data to otp success.\n", index);
	}

	/* should set esck read lock */
	res = rk_esck_set_read_lock(key_id);
	if (res) {
		EMSG("Set ESCK%d read lock failed.\n", index);
		goto exit;
	}

	IMSG("Set ESCK%d read lock success.\n", index);
exit:
	IMSG("\n");

	return res;
}

static TEE_Result rk_keylad_test_write_otpkey(uint32_t key_id, uint8_t *key, uint32_t key_len)
{
	TEE_Result res = TEE_SUCCESS;
	bool is_written = false;
	int index = key_id - RK_OEM_OTP_KEY0;

	IMSG("Start write OEM KEY%d data to otp...\n", index);

	res = rk_is_oem_otp_key_written(key_id, &is_written);
	if (res == TEE_ERROR_NOT_SUPPORTED) {
		IMSG("This platform do not support otp key. Skip.");
		goto exit;
	} else if (res == TEE_ERROR_ACCESS_DENIED) {
		IMSG("OEM KEY%d is locked skip write.\n", index);
		goto exit;
	} else if (res) {
		EMSG("Check OEM KEY%d is written failed.\n", index);
		goto exit;
	}

	if (!is_written) {
		ta_dump_hex("OEM KEY", key, key_len);
		res = rk_write_oem_otp_key(key_id, key, key_len);
		if (res) {
			EMSG("Write OEM KEY%d is written failed.\n", index);
			goto exit;
		}

		IMSG("Write OEM KEY%d data to otp success.\n", index);
	}

	/* should set oem otp key read lock */
	res = rk_oem_otp_key_set_read_lock(key_id);
	if (res) {
		EMSG("Set OEM KEY%d read lock failed.\n", index);
		goto exit;
	}

	IMSG("Set OEM KEY%d read lock success.\n", index);
exit:
	IMSG("\n");

	return res;
}

TEE_Result handle_keylad_write(void)
{
	rk_keylad_test_write_esck(g_test_esck_key_id, (uint8_t *)esck_data, sizeof(esck_data));

	rk_keylad_test_write_otpkey(g_test_oem_key_id, (uint8_t *)otp_key1, sizeof(otp_key1));

	return TEE_SUCCESS;
}

static TEE_Result rk_keylad_hmac_test(uint32_t algo, const char *name)
{
	TEE_Result res;
	uint32_t otp_keyid = g_test_oem_key_id;
	uint32_t key_len = sizeof(otp_key1);
	uint8_t *key = (uint8_t *)otp_key1;
	uint8_t msg[] = {
		0xef, 0x5e, 0xa3, 0xaf, 0x1d, 0x3b, 0x8d, 0x18,
		0xa2, 0x30, 0x68, 0x66, 0x78, 0x68, 0x59, 0x2a,
	};
	uint32_t msg_len = sizeof(msg);
	uint8_t mac_hard[64], mac_soft[64];
	uint32_t mac_hard_len = sizeof(mac_hard);
	uint32_t mac_soft_len = sizeof(mac_soft);

	res = rk_mac_crypto((uint8_t *)msg, mac_soft, msg_len, &mac_soft_len,
			    key, key_len, NULL, algo);
	if (res) {
		EMSG("rk_hmac_soft failed.\n");
		goto exit;
	}

	res = rk_keylad_hmac(algo, otp_keyid, key_len, msg, msg_len, mac_hard, &mac_hard_len);
	if (res) {
		EMSG("rk_keylad_hmac failed.\n");
		goto exit;
	}

	if (mac_hard_len != mac_soft_len ||
		memcmp(mac_hard, mac_soft, mac_hard_len)) {
		EMSG("hmac value mismatch!\n");
		res = TEE_ERROR_MAC_INVALID;
	}

exit:
	if(res == 0)
		IMSG("%-16s using RK_OEM_OTP_KEY%u test success.\n", name, otp_keyid);
	else if (res == TEE_ERROR_NOT_SUPPORTED)
		IMSG("%-16s using RK_OEM_OTP_KEY%u test not support.\n", name, otp_keyid);
	else
		IMSG("%-16s using RK_OEM_OTP_KEY%u test failed.\n", name, otp_keyid);

	return res;
}

TEE_Result handle_keylad_hmac(void)
{
	TEE_Result res = TEE_SUCCESS;
	bool is_key_written = false;

	IMSG("---------------------------- %s start --------------------------\n", __func__);

	res = rk_is_oem_otp_key_written(g_test_oem_key_id, &is_key_written);
	if (res == TEE_ERROR_NOT_SUPPORTED) {
		IMSG("This platform do not support otp key. Skip.");
		goto exit;
	} else if (res == TEE_ERROR_ACCESS_DENIED) {
		IMSG("OEM KEY%u is locked.\n", g_test_oem_key_id);
	} else if (res) {
		EMSG("Check OEM KEY%u is written failed.\n", g_test_oem_key_id);
		goto exit;
	}

	if (!is_key_written && res != TEE_ERROR_ACCESS_DENIED) {
		IMSG("Please write OEM KEY%u first [ rktest keylad_write ] !!!!!!!!!!!\n",
		     g_test_oem_key_id);
		res = TEE_ERROR_BAD_STATE;
		goto exit;
	}

	rk_keylad_hmac_test(TEE_ALG_HMAC_MD5,    "HMAC_MD5");
	rk_keylad_hmac_test(TEE_ALG_HMAC_SHA1,   "HMAC_SHA1");
	rk_keylad_hmac_test(TEE_ALG_HMAC_SHA256, "HMAC_SHA256");
	rk_keylad_hmac_test(TEE_ALG_HMAC_SHA512, "HMAC_SHA512");
	rk_keylad_hmac_test(TEE_ALG_HMAC_SM3,    "HMAC_SM3");

	res = TEE_SUCCESS;
exit:
	IMSG("---------------------------- %s finish --------------------------\n", __func__);

	return res;
}
