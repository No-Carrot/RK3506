/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 */
#ifndef _EC_KEY_DATA_H_
#define _EC_KEY_DATA_H_

#include "rkcrypto_common.h"

RK_RES test_init_ec_pubkey(rk_ec_pub_key_pack *pub, enum RK_EC_CURVE curve);
RK_RES test_init_ec_privkey(rk_ec_priv_key_pack *priv, enum RK_EC_CURVE curve);

#endif /*_TEST_EC_H_*/