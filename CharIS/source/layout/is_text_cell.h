#pragma once
#include <CharIS/include/is_base.h>
#include <CharIS/include/is_font.h>
#include <CharIS/include/is_text_layout.h>
#include <CharIS/include/is_Inline_object.h>
#include "../core/is_common.h"
#include "../renderer/is_text_renderer_common.h"

namespace CharIS {

    struct IISFontFace;
    struct IISFontFaceGlyph;
    struct IISStringU16;
    class CISTextRenderer;

#ifdef NDEBUG
    enum : uint32_t { TEXT_CELL_MAXLEN = 64 };
#else
    enum : uint32_t { TEXT_CELL_MAXLEN = 11 };
#endif


    /// <summary>
    /// 
    /// </summary>
    struct alignas(uint16_t) TextCellBoolSet {

        bool            dirty;
        // cache validity
        bool            cache;

    };


    /// <summary>
    /// 
    /// </summary>
    struct TextEnvironment {
        // defualt effect
        TextEffect              defaultEffect;

        uint32_t                selForeground;

        uint32_t                selBackground;

        fp26dot6_t              lineHeightAscent;

        fp26dot6_t              lineHeightDescent;
        // kerning
        bool                    kerning;

    };

    /// <summary>
    /// 
    /// </summary>
    struct TextDocport {

        fp26dot6_t                      left;

        fp26dot6_t                      top;

        fp26dot6_t                      right;

        fp26dot6_t                      bottom;

    };

    /// <summary>
    /// 
    /// </summary>
    class CISTextCell final : public Node {

    public:

        CISTextCell(IISFontFace*, const TextEnvironment* env, fp26dot6_t size = 12 << 6) noexcept;

        ~CISTextCell() noexcept;

        CISTextCell(const CISTextCell&) noexcept;

    public:

        const BaseCellBox& RefBox() noexcept;

        auto& RefFontFace() const noexcept { return *m_pTextFont; }

    public:

        struct WeakInsert { uint32_t front, back; };

        WeakInsert InsertTextWeak(uint32_t pos, const char16_t* str, uint32_t len = uint32_t(-1), Node* line = nullptr) noexcept;

        void HitTest(uint32_t pos, HitTestCtx& ctx) noexcept;

        void HitTest(fp26dot6_t pos, HitTestCtx& ctx) noexcept;

        uint32_t AppendText(const char16_t* str, uint32_t len = uint32_t(-1)) noexcept;

        void RemoveText(Range) noexcept;

        void SetEffectFlagOn(uint32_t begin, uint32_t end, uint32_t bit) noexcept;

        void SetEffectFlagOff(uint32_t begin, uint32_t end, uint32_t bit) noexcept;

        void SetSubEffect(uint32_t begin, uint32_t end, uint32_t offset, uint32_t value) noexcept;

        void SetKerning() noexcept;

        bool PreSetFont(fp26dot6_t size = 0, IISFontFace* = nullptr) const noexcept;

        bool SetFont(fp26dot6_t size = 0, IISFontFace* = nullptr) noexcept;

        auto GetFontSize() const noexcept { return m_fpFontSize; }

        auto GetView() const noexcept { return U16View{ m_aText, m_uTextCount }; }

        auto GetPosition() const noexcept { return m_aPosition; }

        //auto GetUnit() const noexcept { return m_iUnitPerEm; }

        auto FindFrontChar(char32_t& first, uint32_t len, bool word) noexcept -> uint32_t;

        auto FindBackChar(char32_t& first, uint32_t offset, bool word) noexcept -> uint32_t;

        void DrawLineFeed(CISTextRenderer*, fppoint_t, void*, TextRenderParam param, const TextDocport* docwport) noexcept;

        TextDraw Draw(CISTextRenderer*, fppoint_t, void*, TextRenderParam param, uint32_t selBegin, uint32_t selEnd, const TextDocport*) noexcept;

        CODE AppendString(IISStringU16&) noexcept;

        void Idle(CISTextRenderer*) noexcept;

    protected:

        uint32_t insertTextFrontFirst(uint32_t pos, const char16_t* str, uint32_t len) noexcept;

        uint32_t insertTextBackFirst(uint32_t pos, const char16_t* str, uint32_t len) noexcept;

        void insertFuncMoveTailEffect(uint32_t count, uint32_t pos) noexcept;

        void drawDecoration() noexcept;

        void drawGlyph() noexcept;

        void clear_cache(CISTextRenderer* renderer) noexcept;

    public:

        void SetInlineObject(IISInlineObject*) noexcept;

        void SetPasswordMode(char32_t ch) noexcept;

        uint32_t SplitCharPos(fp26dot6_t size, WRAP_MODE mode, bool first) noexcept;
        // always create one
        CISTextCell* SplitStrong(uint32_t pos) noexcept;

        // return this, if pos == 0 
        // return next, if pos >= len
        // else: return new (nullptr oom)
        CISTextCell* SplitWeak(uint32_t pos) noexcept;

        // check format with next cell, return true if same
        // same format: same font/size
        bool SameNext(Node* end) const noexcept;

        // merge this with next cell, return true if success
        bool MergeNext(Node* end) noexcept;

    protected:

        //bool isLast() const noexcept;

        void clean() noexcept { m_bools.dirty = false; }

        void dirty() noexcept { m_bools.dirty = true; }

        bool is_dirty() const noexcept { return m_bools.dirty; }

        void relayout() noexcept;

        void checkLayout() noexcept;

        void invalidate_cache() noexcept { m_bools.cache = false; }

    public:

        fppoint_t               position = {  };

        fp26dot6_t              head = 0;

        fp26dot6_t              tail = 0;

    protected:

        CISTextRenderer*        m_pTextRendererWeak = nullptr;

        const TextEnvironment*  m_pTextEnv = nullptr;
#ifndef NDEBUG
        size_t                  m_dbgCode = 0x87654321;
#endif
        IISFontFace*           m_pTextFont = nullptr;

        IISInlineObject*        m_pInlineObject = nullptr;

        fp26dot6_t              m_fpFontSize = 12 << 6;

        BaseCellBox             m_boxCell = {};

        //int32_t                 m_iUnitPerEm = 0;
        // pos
        int32_t                 m_aPosition[TEXT_CELL_MAXLEN + 1] = {};

        //CODE                  m_eLastError = CODE_OK;

        uint32_t                m_uTextCount = 0;

        char32_t                m_chPassword = 0;

    protected:
        // Unicode
        char16_t                m_aText[TEXT_CELL_MAXLEN] = {};

        char16_t                m_chRev = 0;

        TextCellBoolSet         m_bools = {};

    protected:
        // cache
        GlyphCache              m_aCache[TEXT_CELL_MAXLEN] = {};
        // effect
        TextEffect              m_aEffects[TEXT_CELL_MAXLEN] = {};

    };

}