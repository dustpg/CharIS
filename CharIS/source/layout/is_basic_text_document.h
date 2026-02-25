#pragma once
#include <CharIS/include/is_text_document.h>
#include "is_basic_layout_engine.h"
#include "../core/is_helper_p.h"
//#include "is_document_command.h"

namespace CharIS { namespace BasicLayout {

    /// <summary>
    /// 
    /// </summary>
    /// <seealso cref="IISTextLayout" />
    class CISTextDocuemnt final : public impl::ref_count_class<CISTextDocuemnt, IISTextDocument> {

        void saveRaisePoint(SELECTION_MODE) noexcept;

    public:

        CISTextDocuemnt(CISTextRenderer* manager, IISFontFace* font, fp26dot6_t size) noexcept;

        ~CISTextDocuemnt() noexcept;

    public: // DOC

        CODE SetAttribute(TEXT_DOC_ATTR attr, int32_t value) noexcept override;

        CODE SetSelection(SELECTION_MODE mode, uint32_t pos, bool keepAnchor) noexcept override;

        CODE GetSelection(TextSelection sel[], uint32_t& count) noexcept override;

        CODE Serialize(IISBlob*, const char* format) noexcept override;

        CODE Deserialize(IISBlob*) noexcept override;

        CODE InsertText(uint32_t position, const char16_t* text, uint32_t len, uint32_t* inserted) noexcept override;

        CODE RemoveText(Range range, uint32_t* removed) noexcept override;

        CODE MakePlainString(Range, IISStringU16* string) noexcept override;

        CODE SetDefualtEffect(const TextEffect&) noexcept override;

        CODE SetInlineObject(Range, IISInlineObject*) noexcept override;

    public: // UNDO

        CODE BeginOp() noexcept override;

        CODE EndOp() noexcept override;

        CODE Undo() noexcept override;

        CODE Redo() noexcept override;

        CODE SetUndoLimit(uint32_t limit) noexcept override;

        CODE ClearUndo() noexcept override;

    public: // LAYOUT

        TextDraw Draw(void* context, TextRenderParam param, const TextViewportRange*) noexcept override;

        CODE SetFontSize(Range range, fp26dot6_t) noexcept override;

        CODE SetFontFace(Range range, const char16_t*, FONT_WEIGHT w, FONT_STYLE se, FONT_STRETCH sh) noexcept override;

        CODE SetSubEffect(Range range, uint32_t offset, uint32_t value) noexcept override;

        CODE SetEffectFlag(Range range, uint32_t bitIndex, bool on) noexcept override;

        CODE SetDirection(TEXT_DIRECTION reading, TEXT_DIRECTION paragraph) noexcept override;

        CODE SetCanvasSize(fpsize_t size, TEXT_COORDINATE_SYSTEM) noexcept override;

        CODE SetViewpoint(fppoint_t pos, TEXT_COORDINATE_SYSTEM) noexcept override;

        CODE GetLayoutSize(fpsize_t& size, TEXT_COORDINATE_SYSTEM ) noexcept override;

        CODE HitTest(fppoint_t point, HitTestCtx&, TEXT_COORDINATE_SYSTEM ) noexcept override;

        CODE HitTest(uint32_t position, HitTestCtx&, TEXT_COORDINATE_SYSTEM ) noexcept override;

    protected:

        CISTextRenderer*        m_pTextRenderManager = nullptr;

        IISFontEngine&          m_refFontEngine;

        CISLayoutEngine         m_oEngine;

        fp26dot6_t              m_fpRaiseLowerPoint = 0;

        uint32_t                m_uAnchor = 0;

        uint32_t                m_uCaret = 0;

    protected:

        Node                    m_sUndoHead = {};

        Node                    m_sUndoTail = {};

        uint32_t                m_uUndoCount = 0;

    protected:

        bool                    m_bRichText = false;

    };



}}
