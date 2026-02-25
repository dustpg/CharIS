#pragma once

#include <CharIS/include/is_base.h>
#include <CharIS/include/is_text_layout.h>
#include <CharIS/include/is_blob_string.h>
#include <CharIS/include/is_Inline_object.h>
#include <CharIS/include/is_font.h>
#include "../layout/is_text_cell.h"
#include "../renderer/is_text_renderer_impl.h"

#include <vector>

namespace CharIS { namespace BasicLayout {

    struct Paragraph {

        CISTextCell*    first;
        // excluding the newline character
        uint32_t        length;

        fpsize_t        size;

        bool            dirty;
    };

    /// <summary>
    /// 
    /// </summary>
    struct LineFeed {
        // (\r)\n
        char16_t    newline[2];
        // 1/2 (\r\n)(\r)(\n)
        uint32_t    nlcount;
    };

    /// <summary>
    /// 
    /// </summary>
    struct BasicTextSetting {

        fpsize_t                canvasSize;

        WRAP_MODE               wrapMode;

        TEXT_ALIGN              textAlign;

        VERTICAL_ALIGN          verticalAlign;

    };

    /// <summary>
    /// 
    /// </summary>
    class CISLayoutEngine final {
        struct FindCtx { 
            CISTextCell* cell; 
            Paragraph* par;
            uint32_t offsetInPar;
            uint32_t offsetInCell;
        };
    public:

        CISLayoutEngine(IISFontFace*, fp26dot6_t size = 12 << 6) noexcept;

        ~CISLayoutEngine() noexcept;

        CODE SetTextAlign(TEXT_ALIGN) noexcept;

        CODE SetWrapMode(WRAP_MODE) noexcept;

        CODE SetVerticalAlign(VERTICAL_ALIGN) noexcept;

    public:

        void Shrink() noexcept;

        void SetCanvasOrigin(fppoint_t) noexcept;

        void SetCanvasSize(fpsize_t) noexcept;

        void HitTest(fppoint_t, HitTestCtx&) noexcept;

        void HitTest(uint32_t pos, HitTestCtx&) noexcept;

        TextDraw Draw(CISTextRenderer*, void* context, TextRenderParam param, const TextViewportRange*) noexcept;

        fpsize_t DocSize() noexcept;

        auto TextLength() const noexcept { return m_cTextLength; }

    public:
        // slope: base width SPACE
        void SetTabWidth(fppoint_t slope, fppoint_t intercept) noexcept;

        void SetSelectionBlock(Range) noexcept;

        void SetSelectionForeground(uint32_t foreground) noexcept { m_sEnv.selForeground = foreground; }

        void SetSelectionBackground(uint32_t background) noexcept { m_sEnv.selBackground = background; }

    public:

        uint32_t FrontChar(uint32_t, bool word) noexcept;

        uint32_t BackChar(uint32_t, bool word) noexcept;

        uint32_t RaiseChar(uint32_t index, fp26dot6_t pos) noexcept;

        uint32_t LowerChar(uint32_t index, fp26dot6_t pos) noexcept;

        uint32_t LineBegin(uint32_t pos) noexcept;

        uint32_t LineEnd(uint32_t pos) noexcept;

    public:

        void SetDefaultEffect(const TextEffect&) noexcept;

        void SetKerning(bool on) noexcept;

        void SetPasswordMode(char32_t ch) noexcept;

        void SetSingleLine(bool on) noexcept;

        void SetLineHeightAscent(fp26dot6_t v) noexcept { m_sEnv.lineHeightAscent = v; }

        void SetLineHeightDescent(fp26dot6_t v) noexcept { m_sEnv.lineHeightDescent = v; }

    public:
        // \r \r\n \n
        CODE SetLineFeed(LineFeed) noexcept;

        CODE SetFont(Range, IISFontFace* face = nullptr, fp26dot6_t size = 0) noexcept;

        CODE MakeString(Range range, IISStringU16&) noexcept;

        CODE RemoveText(Range range) noexcept;
        // (\r\n) (\n) support
        CODE InsertText(uint32_t pos, const char16_t* str, uint32_t len = uint32_t(-1)) noexcept;

        CODE SetSubEffect(Range range, uint32_t offset, uint32_t value) noexcept;

        CODE SetEffectFlag(Range range, uint32_t bitNum, bool on) noexcept;

        CODE SetInlineObject(Range range, IISInlineObject* obj) noexcept;

    protected:

        bool doShrink() noexcept;

        void dirtyAllParagraphs() noexcept;

        template<typename T> void dirtyAll(T func) noexcept;

        template<typename T> CODE effect(Range range, T func) noexcept;

    protected:

        CODE makeRangedString(Range range, IISStringU16&) noexcept;

        CODE newLine(uint32_t pos) noexcept;

        CODE insertLine(uint32_t pos, const char16_t* str, uint32_t len) noexcept;

        bool findPos(uint32_t, FindCtx&) noexcept;

        bool baseParagraph() noexcept;

        void relayout() noexcept;

        void checkLayout() noexcept;

        void removeAll() noexcept;

        void relayoutTextAlign() noexcept;

    protected:

        void paragraphRelayout(Paragraph&) noexcept;

        void paragraphCheckLayout(Paragraph&) noexcept;

    protected:

        IISFontFace*                m_pDefaultFont = nullptr;
        // +1 LINE[TAIL]
        std::vector<Paragraph>      m_vParagraphs;

        LineFeed                    m_sLineFeed;

        fppoint_t                   m_ptOrigin = {};
            
        uint32_t                    m_cTextLength = 0;

        fp26dot6_t                  m_fpDefaultSize;

        BasicTextSetting            m_sSetting = {};

        fpsize_t                    m_fsDocSize = {};

        Range                       m_sSelectionRange = {};

        char32_t                    m_chPassword = 0;

        Node                        m_sHead;

        Node                        m_sTail;

        TextEnvironment             m_sEnv = {};
#ifndef NDEBUG
        char                        dbg_buffer[sizeof(CISTextCell)];
#endif 
        bool                        m_bDirty = false;

        bool                        m_bSingleLine = false;

        bool                        m_bNeedShrink = false;

    };

}}