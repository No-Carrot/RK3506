/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 */

#ifndef __RKCRYPTO_EC_HELPER_H__
#define __RKCRYPTO_EC_HELPER_H__

#include <stdint.h>
#include "rkcrypto_common.h"

/*
 * The rk_ec_pub_key will be encoded in BER format as follows
 */
RK_RES rk_ec_pubkey_encode(rk_ec_pub_key_pack *pub,
			  uint8_t *asn1_key, uint16_t *asn1_key_len, uint16_t *key_bits);

/*
 * The rk_ec_priv_key will be encoded in BER format as follows
 */
RK_RES rk_ec_privkey_encode(rk_ec_priv_key_pack *priv,
			    uint8_t *asn1_key, uint16_t *asn1_key_len, uint16_t *key_bits);

#endif
