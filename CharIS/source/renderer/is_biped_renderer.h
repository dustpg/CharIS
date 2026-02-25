#pragma once

#include <CharIS/include/is_base.h>
#include <CharIS/include/is_font_face.h>
#include <CharIS/include/is_text_renderer.h>
#include "biped.h"
#include "../utils/is_pod_queue.h"
#include "is_text_renderer_common.h"
#include "is_family_id.h"

namespace CharIS {

    enum BITMAP_TYPE : uint32_t {
        BITMAP_TYPE_GRAY = 0,
        BITMAP_TYPE_RGBH,
        BITMAP_TYPE_BGRH,
        BITMAP_TYPE_RGBV,
        BITMAP_TYPE_BGRV,
    };

    enum : uint32_t {
        MSDF_SIZE_L = 64,
        MSDF_SIZE_M = 32,
        MSDF_SIZE_Z = 0,
        MSDF_PADDING = 2,
        MSDF_THRESHOLD_VALUE = 64,
    };

    struct IISGraphics;
    class CISTextRenderer;

    using BipedKey = charis_key_t;

    enum BIPED_STATE : uint8_t {
        BIPED_STATE_PENDING = 0,
        BIPED_STATE_READY,
    };

    struct alignas(uint32_t) BipedValue {

        GLYPH_CACHE_TYPE    type;

        BIPED_STATE         state;
        // TODO: [w, h] -> [h, w]
        uint8_t             rotated;
        // TODO: scale down factor
        uint8_t             scale;
        // fp10.6 
        int16_t             offsetX;
        // fp10.6 
        int16_t             offsetY;

    };


    struct BipedKeyValue {

        BipedKey        key;

        BipedValue      value;

    };

    class CISBipedRenderer final {

    public:

        CISBipedRenderer(CISTextRenderer* parent) noexcept;

        ~CISBipedRenderer() noexcept;

        CODE Init(uint32_t size, bool rgb, bool a) noexcept;

        void TaskDone(uint64_t) noexcept;

        void ClearCache(GlyphCache list[], size_t len) noexcept;

        bool DrawCache(GlyphCache cache, const TextEffect& effect, void* context, fp26dot6_t size, fppoint_t point) noexcept;

        //bool DrawGlyph(IISFontFace* face, const TextEffect& effect, void* context, char32_t ch, fp26dot6_t size, fppoint_t point) noexcept;

        GlyphCache CreateCache(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept;

    protected: // gray / subpixel

        GlyphCache create_cache_bitmap(BITMAP_TYPE type, IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept;

        GlyphCache add_task_bitmap(IISFontFace* face, BITMAP_TYPE type, biped_cache_ctx_t ctx) noexcept;

    protected:

        bool draw_glyph_gray(const biped_block_info_t& block, fp26dot6_t size, fppoint_t point, const TextEffect& effect, void* context) noexcept;

        GlyphCache add_task_gray(IISFontFace* face) noexcept;

        GlyphCache create_cache_gray(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept;

    protected:

        bool draw_glyph_subp(const biped_block_info_t& block, fp26dot6_t size, fppoint_t point, const TextEffect& effect, void* context) noexcept;

        GlyphCache create_cache_subp(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept;

        GlyphCache add_task_subp(IISFontFace* face) noexcept;

    protected:

        bool draw_glyph_msdf(const biped_block_info_t& block, fp26dot6_t size, fppoint_t point, const TextEffect& effect, void* context) noexcept;

        GlyphCache create_cache_msdf(IISFontFace* face, char32_t ch) noexcept;

        GlyphCache add_biped_task_msdf(IISFontFace* face, uint16_t msdf) noexcept;

    protected:

        uint64_t                    m_hTextureR = 0;

        uint64_t                    m_hTextureRgb = 0;

        CISTextRenderer*            m_pParent = nullptr;

        IISGraphics*                m_pGraphicsApi = nullptr;

        biped_cache_ctx_t           m_ctxBipedRgb = nullptr;

        biped_cache_ctx_t           m_ctxBipedR = nullptr;

        BipedKeyValue               m_sKeyValue = { };

        //uint32_t                    m_uSide = 1;
    };

}