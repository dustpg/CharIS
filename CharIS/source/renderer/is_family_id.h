#pragma once

#include "../core/is_helper_c.h"
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#else
#define noexcept
#endif

#define CHARIS_FAMILY_MAX_LEN 64


typedef struct {

    // 0 ~ 0x10ffff (21bit)
    uint32_t        ch : 21;
    // 8dot3 (256 - 1/8)
    uint32_t        size : 11;
    // family id
    uint16_t        fid;

    uint16_t        weight : 10;

    uint16_t        stretch : 4;

    uint16_t        style : 2;

} charis_key_t;

typedef struct {
    uint32_t    hash;
    uint16_t    id;
    uint16_t    length;
    uint16_t    family[CHARIS_FAMILY_MAX_LEN + 1];
} charis_family_id_slot_t;

typedef struct {
    charis_family_id_slot_t* slots;
    uint32_t length_of_slots;
    uint32_t item_count;
} charis_family_id_ctx_t;

static inline void charis_family_id_ctx_init(charis_family_id_ctx_t* ctx) noexcept {
    memset(ctx, 0, sizeof(*ctx));
}

static inline void charis_family_id_ctx_uninit(charis_family_id_ctx_t* ctx) noexcept {
    if (ctx->slots) {
        charis_free(ctx->slots);
        ctx->slots = NULL;
        ctx->length_of_slots = 0;
    }
}

uint16_t charis_family_to_id(charis_family_id_ctx_t*, const uint16_t* family) noexcept;


#ifdef __cplusplus
}
#endif