#include "is_family_id.h"
#define BIPED_C_IMPLEMENTATION
#define biped_malloc charis_malloc
#define biped_free charis_free
#define biped_realloc charis_realloc
#define biped_hash charis_key_hash
#define biped_min_w (biped_unit_t)4
#define biped_min_h (biped_unit_t)8
uint32_t charis_key_hash(const uint32_t[], uint32_t count);
#include "biped.h"
#include <assert.h>
#include <string.h>

#ifndef CHARIS_FAMILY_INIT_SLOTS
#define CHARIS_FAMILY_INIT_SLOTS 32
#endif

uint32_t charis_key_hash(const uint32_t key[], uint32_t count) {
    assert(count == sizeof(charis_key_t) / sizeof(uint32_t));
    // fixed length for compiler optimization
    return biped_wyhash32(key, sizeof(charis_key_t) / sizeof(uint32_t), 0);
}

static uint32_t charis_family_hash(charis_family_id_slot_t* slot, const uint16_t* family) {
    uint32_t i = 0;
    do {
        slot->family[i] = family[i];
    } while (family[i++] != 0);

    uint32_t length = i - 1;
    uint32_t u32_count = (length + 2) / 2;
    slot->length = (uint16_t)length;
    slot->id = 0;

    return slot->hash = biped_hash_impl((const uint32_t*)slot->family, u32_count);
}

/** Directly compare two slots; returns 1 if equal, 0 otherwise. */
static int charis_family_eq(const charis_family_id_slot_t* a, const charis_family_id_slot_t* b) {
    if (a->hash != b->hash || a->length != b->length) return 0;
    for (uint32_t i = 0; i <= a->length; i++) {
        if (a->family[i] != b->family[i]) return 0;
    }
    return 1;
}

// static uint32_t charis_family_strlen(const uint16_t* family) {
//     uint32_t i = 0;
//     while (family[i] != 0) i++;
//     return i;
// }

static int charis_family_try_rehash(charis_family_id_ctx_t* ctx) {
    if (ctx->slots == NULL) return 1;
    if (ctx->length_of_slots == 0) return 1;
    /* load factor 0.5: rehash when (item_count+1) * 2 >= slot_count */
    if (ctx->item_count * 2u < ctx->length_of_slots) return 1;

    uint32_t new_cap = ctx->length_of_slots * 2;
    charis_family_id_slot_t* new_slots = (charis_family_id_slot_t*)charis_malloc(new_cap * sizeof(charis_family_id_slot_t));
    if (!new_slots) return 0;
    memset(new_slots, 0, new_cap * sizeof(charis_family_id_slot_t));


    const uint32_t mask = new_cap - 1;
    for (uint32_t i = 0; i < ctx->length_of_slots; i++) {
        charis_family_id_slot_t* s = &ctx->slots[i];
        if (s->id == 0) continue;
        uint32_t h = s->hash;
        for (uint32_t j = 0; j < new_cap; j++) {
            uint32_t k = (h + j) & mask;
            if (new_slots[k].id == 0) {
                new_slots[k] = *s;
                break;
            }
        }
    }
    charis_free(ctx->slots);
    ctx->slots = new_slots;
    ctx->length_of_slots = new_cap;
    return 1;
}

uint16_t charis_family_to_id(charis_family_id_ctx_t* ctx, const uint16_t* family) {
    assert(ctx && family);

    //uint32_t length = charis_family_strlen(family);
    //if (length > CHARIS_FAMILY_MAX_LEN) return 0;
    ///* empty string: treat as invalid, return 0 */
    //if (length == 0) return 0;

    /* lazy init */
    if (!ctx->slots) {
        uint32_t cap = CHARIS_FAMILY_INIT_SLOTS;
        assert((cap & (cap - 1)) == 0 && "init slots must be power of 2");
        ctx->slots = (charis_family_id_slot_t*)charis_malloc(cap * sizeof(charis_family_id_slot_t));
        if (!ctx->slots) return 0;
        ctx->length_of_slots = cap;
        ctx->item_count = 0;
        memset(ctx->slots, 0, cap * sizeof(charis_family_id_slot_t));
    }

    charis_family_id_slot_t slot;
    const uint32_t hash = charis_family_hash(&slot, family);


    const uint32_t mask = ctx->length_of_slots - 1;

    /* search existing */
    for (uint32_t i = 0; i < ctx->length_of_slots; i++) {
        uint32_t j = (hash + i) & mask;
        charis_family_id_slot_t* s = &ctx->slots[j];
        if (s->id == 0) break;
        if (charis_family_eq(&slot, s)) {
            return s->id;
        }
    }

    /* not found: rehash if load >= 0.5 */
    if (!charis_family_try_rehash(ctx)) return 0;

    /* id full (0 reserved, 1..0xFFFF usable): cannot insert */
    if (ctx->item_count >= 0xFFFF) return 0;

    /* insert */
    ctx->item_count++;
    uint16_t new_id = (uint16_t)ctx->item_count;
    {
        const uint32_t insert_mask = ctx->length_of_slots - 1;
        for (uint32_t i = 0; i < ctx->length_of_slots; i++) {
            uint32_t j = (hash + i) & insert_mask;
            if (ctx->slots[j].id == 0) {
                memcpy(&ctx->slots[j], &slot, sizeof(charis_family_id_slot_t));
                ctx->slots[j].id = new_id;
                return new_id;
            }
        }
    }
    assert(!"hash table full");
    return 0;
}
