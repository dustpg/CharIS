#pragma once

#include <CharIS/include/is_font_engine.h>
#include <CharIS/include/is_text_renderer.h>
#include <CharIS/include/is_graphics_api.h>
#include "../core/is_helper_p.h"
#include "is_biped_renderer.h"
#include "is_polygon_renderer.h"
#include "is_family_id.h"
#include "../utils/is_thread_pool.h"

namespace CharIS {
    
    class CISTextRenderer : public impl::ref_count_class<CISTextRenderer, IISTextRenderer> {
    public:

        auto RefFontEngine() noexcept -> IISFontEngine& { return *m_pFontEngine; }

        auto RefGraphicsApi() noexcept -> IISGraphics& { return *m_pGraphicsApi; }

        auto RefThreadPool() noexcept -> CISThreadPool& { return m_oThreadPool; }

    public:

        ~CISTextRenderer() noexcept;

        CISTextRenderer(IISFontEngine* engine, IISGraphics* graphics) noexcept;

        uint16_t FamilyToId(const char16_t* family) noexcept;

        CODE Init(const CreateTextRendererParam&) noexcept;

        void Update(const uint64_t* ids, size_t length) noexcept override;

        void ClearCache(GlyphCache list[], size_t len) noexcept;

    public:

        void FillRect(uint32_t background, void* context, TextRenderParam param, fppoint_t point, fpsize_t size) noexcept ;

        bool DrawGlyph(IISFontFace* face, const TextEffect& effect, GlyphCache& cache, void* context, TextRenderParam param, char32_t ch, fp26dot6_t size, fppoint_t point) noexcept;

        bool DrawInlineObject(IISInlineObject*, fppoint_t point, void* context, TextRenderParam param) noexcept;

        bool SetAttribute(const char* attr, int32_t value) noexcept;

    protected:

        charis_family_id_ctx_t      m_ctxFamilyId;

        CISThreadPool               m_oThreadPool;

        IISFontEngine*              m_pFontEngine = nullptr;

        IISGraphics*                m_pGraphicsApi = nullptr;

        CISBipedRenderer            m_oBipedRenderer;

        CISPolygonRenderer          m_oPolygonRenderer;

    };

    inline uint16_t CISTextRenderer::FamilyToId(const char16_t* family) noexcept
    {
        return charis_family_to_id(&m_ctxFamilyId, reinterpret_cast<const uint16_t*>(family));
    }

};