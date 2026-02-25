#pragma once
#include <cstdint>

namespace CharIS {

    enum GLYPH_TASK_TYPE : uint8_t {
        GLYPH_TASK_TYPE_NONE = 0,
        GLYPH_TASK_TYPE_R,
        GLYPH_TASK_TYPE_RGB_SUBP,
        GLYPH_TASK_TYPE_RGB_MSDF,
    };

    union GlyphTaskId {
        uint64_t                id;
        struct {
            GLYPH_TASK_TYPE     type;
            int8_t              offsetX8;    // 8.0
            uint16_t            bid;
            uint8_t             width;
            uint8_t             height;
            int16_t             offsetY16;    // 10.6
        } gray;
        struct {
            GLYPH_TASK_TYPE     type;
            uint8_t             side;
            uint16_t            bid;
            int16_t             offsetX;    // 10.6
            int16_t             offsetY;    // 10.6
        } msdf;
    };

    static_assert(sizeof(GlyphTaskId) == sizeof(uint64_t), "bad id");


    enum GLYPH_CACHE_TYPE : uint8_t {
        GLYPH_CACHE_TYPE_NONE = 0,
        GLYPH_CACHE_TYPE_R_GRAY,
        GLYPH_CACHE_TYPE_RGB_SUBP,
        GLYPH_CACHE_TYPE_RGB_MSDF_L,
        GLYPH_CACHE_TYPE_RGB_MSDF_M,
    };


    union GlyphCache {
        uint32_t                raw;
        struct {
            GLYPH_CACHE_TYPE    type;
            uint8_t             unused;
            uint16_t            bid;
        } biped;
    };

};