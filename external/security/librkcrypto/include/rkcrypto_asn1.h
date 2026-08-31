/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 */

#ifndef __RKCRYPTO_ASN1_H__
#define __RKCRYPTO_ASN1_H__

#include <stdint.h>
#include "rkcrypto_common.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define ASN1_INTEGER			((uint8_t)0x02)
#define ASN1_BIT_STRING			((uint8_t)0x03)
#define ASN1_OCT_STRING			((uint8_t)0x04)
#define ASN1_NULL			((uint8_t)0x05)
#define ASN1_OBJ_IDENTIFIER		((uint8_t)0x06)
#define ASN1_SEQUENCE			((uint8_t)0x30)
#define ASN1_CONTEXT0			((uint8_t)0xA0)
#define ASN1_CONTEXT1			((uint8_t)0xA1)

typedef struct {
	const uint8_t	*data;		//the buffer of data
	uint16_t	*data_len;	//valid length of data
	uint8_t		tag;		//ASN1 data type
	uint8_t		need_plus;	//to identify weather the data is a positive number
} asn1_object_t;

struct hash_oid_item {
	uint32_t	hash_algo;
	const uint8_t	*oid;
	uint32_t	oid_size;
};

RK_RES get_oid_by_md(uint32_t hash_algo, const uint8_t **oid, uint32_t *old_len);

RK_RES asn1_compose_len(uint32_t len, uint8_t *field, uint32_t *field_len);

RK_RES asn1_set_object(const uint8_t *in, uint32_t in_len, uint8_t tag, uint8_t need_plus,
		       uint8_t *out, uint32_t out_max, uint32_t *out_len);

#endif