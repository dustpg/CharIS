#pragma once

#include <CharIS/include/is_base.h>
#include <CharIS/include/is_font_face.h>
#include <CharIS/include/is_text_renderer.h>
#include "../utils/is_pod_queue.h"
#include "is_text_renderer_common.h"
#include "is_family_id.h"

namespace CharIS {

    class CISTextRenderer;

    class CISPolygonRenderer final {

    public:

        CISPolygonRenderer(CISTextRenderer* parent) noexcept;

        ~CISPolygonRenderer() noexcept;

        CODE Init(uint32_t limit) noexcept;

        void TaskDone(uint64_t) noexcept;

        void ClearCache(GlyphCache list[], size_t len) noexcept;

        bool DrawCache(GlyphCache cache, const TextEffect& effect, void* context, fp26dot6_t size, fppoint_t point) noexcept;

        GlyphCache CreateCache(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept;

    protected:

        GlyphCache create_cache_polygon(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept;

        CISTextRenderer*            m_pParent = nullptr;
    };
}