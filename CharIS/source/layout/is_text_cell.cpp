#include "is_text_cell.h"
#include <CharIS/include/is_base.h>
#include <CharIS/include/is_text_layout.h>
#include <CharIS/include/is_blob_string.h>
#include <CharIS/include/is_Inline_object.h>
#include <CharIS/include/is_font.h>
#include <CharIS/include/is_font_face.h>
#include <cstdio>
#include <algorithm>
#include <cassert>
#include <cstring>


#include "../renderer/is_text_renderer_impl.h"

namespace CharIS {

    // Unicode utility functions
    namespace {
        inline bool IsHighSurrogate(char16_t ch) noexcept {
            return ch >= 0xD800 && ch <= 0xDBFF;
        }

        inline bool IsLowSurrogate(char16_t ch) noexcept {
            return ch >= 0xDC00 && ch <= 0xDFFF;
        }

        inline char32_t UCS4FromChar16x2(char16_t high, char16_t low) noexcept {
            return 0x10000 + ((char32_t(high) & 0x3FF) << 10) + (char32_t(low) & 0x3FF);
        }

        inline bool IsSpace(char32_t ch) noexcept {
            return ch == 0x20 || ch == 0x09 || ch == 0x0A || ch == 0x0D || ch == 0x0C || ch == 0x0B;
        }

        inline bool IsCjk(char32_t ch) noexcept {
            return (ch >= 0x4E00 && ch <= 0x9FFF) ||   // CJK Unified Ideographs
                (ch >= 0x3400 && ch <= 0x4DBF) ||   // CJK Extension A
                (ch >= 0x20000 && ch <= 0x2A6DF) || // CJK Extension B
                (ch >= 0x2A700 && ch <= 0x2B73F) || // CJK Extension C
                (ch >= 0x2B740 && ch <= 0x2B81F) || // CJK Extension D
                (ch >= 0x2B820 && ch <= 0x2CEAF) || // CJK Extension E
                (ch >= 0xF900 && ch <= 0xFAFF) ||   // CJK Compatibility Ideographs
                (ch >= 0x2F800 && ch <= 0x2FA1F);    // CJK Compatibility Ideographs Supplement
        }

        inline uint32_t Length(const char16_t* str) noexcept {
            uint32_t n = 0;
            while (str[n]) ++n;
            return n;
        }
    }
}
using namespace CharIS;

/// <summary>
/// Checks the layout.
/// </summary>
/// <returns></returns>
inline void CISTextCell::checkLayout() noexcept
{
    if (this->is_dirty()) {
        this->clean();
        this->relayout();
    }
}

/// <summary>
/// Appends the text.
/// </summary>
/// <param name="str">The string.</param>
/// <param name="len">The length.</param>
/// <returns></returns>
uint32_t CISTextCell::AppendText(const char16_t * str, uint32_t len) noexcept
{
    if (len == uint32_t(-1))
        len = Length(str);
    return this->insertTextFrontFirst(m_uTextCount, str, len);
}

/// <summary>
/// Initializes a new instance of the <see cref="CISTextCell" /> class.
/// </summary>
/// <param name="face">The face.</param>
/// <param name="env">The env.</param>
/// <param name="size">The size.</param>
CISTextCell::CISTextCell(IISFontFace* face, const TextEnvironment* env, fp26dot6_t size) noexcept
    : Node({})
    , m_pTextEnv(env)
    , m_pTextFont(face)
    , m_fpFontSize(size)
{
    this->dirty();
    this->invalidate_cache();
    std::memset(m_aText, 0, sizeof(m_aText));
    std::memset(m_aPosition, 0, sizeof(m_aPosition));
    for (auto &e : m_aEffects)
        e = env->defaultEffect;
    assert(face);
    face->AddRefCnt();
}


/// <summary>
/// Initializes a new instance of the <see cref="CISTextCell" /> class.
/// </summary>
/// <param name="cell">The cell.</param>
CISTextCell::CISTextCell(const CISTextCell & cell) noexcept
    : CISTextCell(cell.m_pTextFont, cell.m_pTextEnv, cell.m_fpFontSize)
{
    const auto p1 = reinterpret_cast<char*>(&m_boxCell);
    const auto p2 = reinterpret_cast<char*>(m_aEffects + TEXT_CELL_MAXLEN);
    const auto len = p2 - p1;
    std::memcpy(&m_boxCell, &cell.m_boxCell, len);

    std::memcpy(&m_aEffects, &cell.m_aEffects, sizeof(m_aEffects));

}

/// <summary>
/// Removes the text.
/// </summary>
/// <param name="range">The range.</param>
/// <returns></returns>
void CISTextCell::RemoveText(Range range) noexcept
{
    if (!range.length)
        return;
    // BACK REMOVE
    if (range.length >= m_uTextCount || range.position + range.length >= m_uTextCount) {
        if (range.position < m_uTextCount) {
            m_uTextCount = range.position;
            // TODO: MINI DIRTY
            this->dirty();
            this->invalidate_cache();
        }
        return;
    }
    // BACK MOVE
    {
        const auto src = m_aText + range.position + range.length;
        const auto dst = m_aText + range.position;
        const auto len = m_uTextCount - range.position - range.length;
        std::memmove(dst, src, sizeof(m_aText[0]) * len);
    }
    {
        const auto src = m_aEffects + range.position + range.length;
        const auto dst = m_aEffects + range.position;
        const auto len = m_uTextCount - range.position - range.length;
        std::memmove(dst, src, sizeof(m_aEffects[0]) * len);
    }

    m_uTextCount -= range.length;
    this->dirty();
    this->invalidate_cache();
}


/// <summary>
/// Finalizes an instance of the <see cref="CISTextCell"/> class.
/// </summary>
/// <returns></returns>
CISTextCell::~CISTextCell() noexcept
{
    // weak ref
    if (m_pTextRendererWeak) {
        this->clear_cache(m_pTextRendererWeak);
        m_pTextRendererWeak = nullptr;
    }
    CharIS::SafeDispose(m_pInlineObject);
    CharIS::SafeDispose(m_pTextFont);
}

/// <summary>
/// Finds the front character.
/// </summary>
/// <param name="first">The first.</param>
/// <param name="len">The length.</param>
/// <returns></returns>
auto CISTextCell::FindFrontChar(char32_t& first, uint32_t len, bool word) noexcept -> uint32_t
{
    char32_t ch = 0;
    for (uint32_t i = 0; i != len; ++i) {
        const uint32_t index = len - i - 1;
        const auto ch16 = m_aText[index];
        ch = ch16;
        // UTF16 -> UCS4
        if (IsLowSurrogate(ch16)) {
            ++i;
            const auto lead = m_aText[index-1];
            assert(IsHighSurrogate(lead));
            ch = UCS4FromChar16x2(lead, ch16);
        }
        // TODO: DO BETTER
        if (word) {
            if (!first)
                first = ch;
            if (IsSpace(first) == IsSpace(ch))
                continue;
            return len - i;
        }
        return len - i - 1;
    }
    return m_uTextCount + 1;
}

/// <summary>
/// Finds the back character.
/// </summary>
/// <param name="first">The first.</param>
/// <param name="offset">The offset.</param>
/// <returns></returns>
auto CISTextCell::FindBackChar(char32_t& first, uint32_t offset, bool word) noexcept -> uint32_t
{
    if (offset < m_uTextCount) {
        char32_t ch = 0;
        const auto len = m_uTextCount;
        for (uint32_t i = offset; i != len; ++i) {
            const auto ch16 = m_aText[i];
            ch = ch16;
            // UTF16->UCS4
            if (IsHighSurrogate(ch16)) {
                const auto trail = m_aText[++i];
                assert(IsLowSurrogate(trail));
                ch = UCS4FromChar16x2(ch16, trail);
            }
            // TODO: DO BETTER
            if (word) {
                if (!first)
                    first = ch;
                if (IsSpace(first) == IsSpace(ch))
                    continue;
                return i;
            }
            return i + 1;
        }
    }
    return m_uTextCount + 1;
}

/// <summary>
/// Hits the test.
/// </summary>
/// <param name="pos">The position.</param>
/// <param name="ctx">The CTX.</param>
/// <returns></returns>
void CISTextCell::HitTest(uint32_t pos, HitTestCtx & ctx) noexcept
{
    assert(pos <= m_uTextCount);
    pos = std::min(pos, m_uTextCount);
    const auto itr = m_aPosition + pos;
    const int32_t unit = FONT_METRICS_UNIT;
    // TODO: CARET
    const auto offset = fp26dot6_t(int64_t(itr[0]) * m_fpFontSize / unit);
    ctx.caret1.x = this->position.x + offset;
    ctx.caret1.y = this->head;
    ctx.caret2.x = ctx.caret1.x;
    ctx.caret2.y = this->tail;
    ctx.postion = uint32_t(itr - m_aPosition);
}

/// <summary>
/// Hits the test.
/// </summary>
/// <param name="pos">The position.</param>
/// <param name="ctx">The CTX.</param>
/// <returns></returns>
void CISTextCell::HitTest(fp26dot6_t pos, HitTestCtx & ctx) noexcept
{
    // BINARY SEARCH: FIND CLOSEST
    const auto findClosest = [=]() noexcept {
        const int32_t unit = FONT_METRICS_UNIT;
        const int32_t target = int32_t(int64_t(pos) * unit / m_fpFontSize);
        const auto begin = m_aPosition;
        const auto end = m_aPosition + m_uTextCount;
        const auto itr = std::lower_bound(begin, end, target);
        // END = OK
        if (itr == begin /*|| itr == end*/)
            return itr;

        const auto before = itr[-1];
        const auto  after = itr[0];
        return (target - before <= after - target) ? itr - 1 : itr;
    };
    const auto itr = findClosest();

    // TODO: CARET
    const int32_t unit = FONT_METRICS_UNIT;
    const auto offset = fp26dot6_t(int64_t(itr[0]) * m_fpFontSize / unit);
    ctx.caret1.x = this->position.x + offset;
    ctx.caret1.y = this->head;
    ctx.caret2.x = ctx.caret1.x;
    ctx.caret2.y = this->tail;
    ctx.postion = uint32_t(itr - m_aPosition);
}

/// <summary>
/// Sets the sub effect.
/// </summary>
/// <param name="begin">The begin.</param>
/// <param name="end">The end.</param>
/// <param name="offset">The offset.</param>
/// <param name="value">The value.</param>
/// <returns></returns>
void CISTextCell::SetSubEffect(uint32_t begin, uint32_t end, uint32_t offset, uint32_t value) noexcept
{
    assert(offset + sizeof(uint32_t) < sizeof(TextEffect));
    assert(end <= TEXT_CELL_MAXLEN);
    for (auto i = begin; i < end; ++i) {
        const auto ptr = reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(m_aEffects + i) + offset);
        *ptr = value;
    }
}

/// <summary>
/// Sets the effect flag on.
/// </summary>
/// <param name="begin">The begin.</param>
/// <param name="end">The end.</param>
/// <param name="bit">The bit.</param>
/// <returns></returns>
void CISTextCell::SetEffectFlagOn(uint32_t begin, uint32_t end, uint32_t bit) noexcept
{
    const uint32_t flag = 1 << bit;
    assert(end <= TEXT_CELL_MAXLEN);
    for (auto i = begin; i < end; ++i) {
        m_aEffects[i].flags |= flag;
    }
}

/// <summary>
/// Sets the effect flag off.
/// </summary>
/// <param name="begin">The begin.</param>
/// <param name="end">The end.</param>
/// <param name="bit">The bit.</param>
/// <returns></returns>
void CISTextCell::SetEffectFlagOff(uint32_t begin, uint32_t end, uint32_t bit) noexcept
{
    const uint32_t flag = ~uint32_t(1 << bit);
    assert(end <= TEXT_CELL_MAXLEN);
    for (auto i = begin; i < end; ++i) {
        m_aEffects[i].flags &= flag;
    }
}


/// <summary>
/// Sizes this instance.
/// </summary>
/// <returns></returns>
const BaseCellBox& CISTextCell::RefBox() noexcept
{
    this->checkLayout();
    return m_boxCell;
}

/// <summary>
/// Sets the kerning.
/// </summary>
/// <param name="on">if set to <c>true</c> [on].</param>
/// <returns></returns>
void CISTextCell::SetKerning() noexcept
{
    this->dirty();
}

/// <summary>
/// Relayouts this instance.
/// </summary>
/// <returns></returns>
void CISTextCell::relayout() noexcept
{
    // INLINE OBJECT
    if (m_pInlineObject) {
        m_pInlineObject->GetCellBox(m_boxCell);
        m_aPosition[0] = 0;
        m_aPosition[1] = m_boxCell.advance;
        //m_iUnitPerEm = m_fpFontSize;
        return;
    }

    assert(m_dbgCode == 0x87654321);
    int32_t width = 0;
    const auto count = m_uTextCount;

    const auto fm = m_pTextFont->GetFontMetrics();
    //const auto units = fm->units;
    const int32_t units = FONT_METRICS_UNIT;
    //m_iUnitPerEm = units;

    CODE code = CODE_OK;
    GlyphMetrics gm;
    const bool kerning = m_pTextEnv->kerning && fm->kerningSupport;

    char32_t last = 0;
    std::memset(m_aPosition, 0, sizeof(m_aPosition));

    for (uint32_t i = 0; i < count; ++i) {
        char16_t ch16 = m_aText[i];
        char32_t ch = ch16;
        m_aPosition[i] = width;
        // UTF16->UCS4
        if (IsHighSurrogate(ch16)) {
            const auto trail = m_aText[++i];
            m_aPosition[i] = width;
            assert(IsLowSurrogate(trail));
            ch = UCS4FromChar16x2(ch16, trail);
        }
        // PASSWORD
        if (m_chPassword)
            ch = m_chPassword;
        // LOCK GLYPH
        IISFontFaceGlyph* glyph = nullptr;
        if (Success(code)) {
            code = m_pTextFont->Lock(ch, &glyph);
        }
        // METRICS
        if (Success(code)) {
            glyph->GetGlyphMetrics(&gm);
        }
        // RUN
        if (Success(code)) {
            width += gm.advanceWidth;
            // KERNING HERE
            if (kerning && last) {
                const auto k = glyph->GetKerning(last, ch);
                m_aPosition[i] += k;
                if (IsHighSurrogate(ch16)) {
                    m_aPosition[i - 1] += k;
                }
                width += k;
            }
            last = ch;
            // UNLOCK GLYPH
            m_pTextFont->Unlock(glyph);
            glyph = nullptr;
        }
    }
    // LAST
    m_aPosition[count] = width;

    m_boxCell.advance = int64_t(width) * int64_t(m_fpFontSize) / int64_t(units);
    m_boxCell.ascent = int64_t(fm->ascent)* int64_t(m_fpFontSize) / int64_t(units);
    m_boxCell.descent = int64_t(fm->descent)* int64_t(m_fpFontSize) / int64_t(units);

}


/// <summary>
/// Splits the strong.
/// </summary>
/// <param name="pos">The position.</param>
/// <returns></returns>
CISTextCell * CISTextCell::SplitStrong(uint32_t pos) noexcept {
    const auto obj = new (std::nothrow) CISTextCell{ *this };
    if (!obj)
        return nullptr;

    // CHECK

    if (pos >= m_uTextCount) {
        obj->m_uTextCount = 0;
        obj->dirty();
        obj->invalidate_cache();
    }
    else {
        // UCS4: +/-1 here
        if (IsLowSurrogate(m_aText[pos])) {
            assert(pos);
            if (pos) pos--;
            else pos++;
            // Warning: split the ucs4 char under utf16
        }
        this->dirty();
        this->invalidate_cache();
        std::memmove(obj->m_aText, obj->m_aText + pos, sizeof(m_aText[0]) * (m_uTextCount - pos));
        std::memmove(obj->m_aEffects, obj->m_aEffects + pos, sizeof(m_aEffects[0]) * (m_uTextCount - pos));

        obj->m_uTextCount = m_uTextCount - pos;
        m_uTextCount = pos;
        obj->dirty();
        obj->invalidate_cache();
    }
    this->Insert(obj);
    return obj;
}

/// <summary>
/// Splits the weak.
/// </summary>
/// <param name="pos">The position.</param>
/// <returns></returns>
CISTextCell * CISTextCell::SplitWeak(uint32_t pos) noexcept
{
    if (!pos)
        return this;
    if (pos >= m_uTextCount)
        return static_cast<CISTextCell*>(this->next);
    return SplitStrong(pos);
}

/// <summary>
/// Sets the inline object.
/// </summary>
/// <param name="obj">The object.</param>
/// <returns></returns>
void CISTextCell::SetInlineObject(IISInlineObject * obj) noexcept
{
    this->dirty();
    this->invalidate_cache();
    CharIS::SafeDispose(m_pInlineObject);
    m_pInlineObject = obj;
    if (obj) {
        obj->AddRefCnt();
    }
}

/// <summary>
/// Sets the passset mode.
/// </summary>
/// <param name="ch">The ch.</param>
/// <returns></returns>
void CISTextCell::SetPasswordMode(char32_t ch) noexcept
{
    if (m_chPassword == ch)
        return;
    m_chPassword = ch;
    this->dirty();
    this->invalidate_cache();
}

/// <summary>
/// Splits the character position.
/// </summary>
/// <param name="size">The size.</param>
/// <param name="mode">The mode.</param>
/// <param name="first">if set to <c>true</c> [first].</param>
/// <returns></returns>
uint32_t CISTextCell::SplitCharPos(fp26dot6_t size, WRAP_MODE mode, bool first) noexcept
{
    this->checkLayout();
    uint32_t flags = 0;
    enum : uint32_t {
        WRAP_FLAG_SPACE = 1 << 0,
        WRAP_FLAG_CJK = 1 << 1,
        WRAP_FLAG_ANY = 1 << 2,
    };
    switch (mode)
    {
    case WRAP_MODE_SPACE_ONLY:
        flags = WRAP_FLAG_SPACE;
        break;
    case WRAP_MODE_SPACE_OR_CJK:
    case WRAP_MODE_ANY_AUTO:
        flags = WRAP_FLAG_SPACE | WRAP_FLAG_CJK;
        break;
    case WRAP_MODE_ANY_WHERE:
        flags = WRAP_FLAG_ANY;
        break;
    }

    const int32_t unit = FONT_METRICS_UNIT;

    const int32_t pos = int32_t(int64_t(size) * unit / m_fpFontSize);


    // BINARY SEARCH? BACK FIRST?
    const auto end = std::find_if(m_aPosition, m_aPosition + m_uTextCount, [pos](int32_t v) noexcept {
        return v > pos;
    });

    if (m_aPosition == end && !first)
        return 0;

    const auto lastIndex = uint32_t(end - m_aPosition);
    // TO FRONT
    for (uint32_t i = 0; i < lastIndex; ++i) {
        const auto index = lastIndex - i - 1;
        char16_t ch16 = m_aText[index];
        char32_t ch = ch16;
        // UTF16->UCS4
        if (IsLowSurrogate(ch16)) {
            ++i;
            const auto lead = m_aText[index-1];
            assert(IsHighSurrogate(lead));
            ch = UCS4FromChar16x2(lead, ch16);
        }
        uint32_t current = WRAP_FLAG_ANY;
        if (IsCjk(ch))
            current |= WRAP_FLAG_CJK;
        if (IsSpace(ch))
            current |= WRAP_FLAG_SPACE;
        // WRAP
        if (current & flags)
            return index + 1;
    }
    // TO BACK
    const auto count = m_uTextCount;
    for (uint32_t i = lastIndex; i < m_uTextCount; ++i) {
        const auto ch16 = m_aText[i];
        char32_t ch = ch16;
        // UTF16->UCS4
        if (IsHighSurrogate(ch16)) {
            const auto trail = m_aText[++i];
            assert(IsLowSurrogate(trail));
            ch = UCS4FromChar16x2(ch16, trail);
        }
        uint32_t current = WRAP_FLAG_ANY;
        if (IsCjk(ch))
            current |= WRAP_FLAG_CJK;
        if (IsSpace(ch))
            current |= WRAP_FLAG_SPACE;
        // WRAP
        if (current & flags)
            return i;
    }
    return 0;
}


/// <summary>
/// Sames this instance.
/// </summary>
/// <returns></returns>
bool CISTextCell::SameNext(Node* end) const noexcept
{
    if (m_pInlineObject)
        return false;
    assert(m_dbgCode == 0x87654321);
    if (this->next == end)
        return false;
    const auto cell = static_cast<CISTextCell*>(this->next);
    assert(cell->m_dbgCode == m_dbgCode);
    if (cell->m_pInlineObject)
        return false;

    return cell->m_pTextFont == m_pTextFont && cell->m_fpFontSize == m_fpFontSize;
}

/// <summary>
/// Merges this instance.
/// </summary>
/// <returns></returns>
bool CISTextCell::MergeNext(Node* end) noexcept
{
    if (this->next == end)
        return false;
    const auto nextCell = static_cast<CISTextCell*>(this->next);
    if (!this->SameNext(end)) {
        // NOT SAME BUT EMPTY
        if (!nextCell->m_uTextCount) {
            nextCell->Remove();
            delete nextCell;
            return true;
        }
        return false;
    }
    // MERGE
    if (m_uTextCount + nextCell->m_uTextCount <= TEXT_CELL_MAXLEN) {
        const auto view = nextCell->GetView();
        this->insertTextBackFirst(m_uTextCount, view.ptr, static_cast<uint32_t>(view.len));
        nextCell->Remove();
        delete nextCell;
        return true;
    }
    return false;
}

/// <summary>
/// Appends the string.
/// </summary>
/// <param name="str">The string.</param>
/// <returns></returns>
CODE CISTextCell::AppendString(IISStringU16 & str) noexcept
{
    const auto view = this->GetView();
    CODE code = str.Append(U16View{ view.ptr, view.len });
    if (m_pInlineObject && Success(code)) {
        code = m_pInlineObject->AppendString(&str);
    }
    return code;
}


void CharIS::CISTextCell::clear_cache(CISTextRenderer* renderer) noexcept
{
    renderer->ClearCache(m_aCache, m_uTextCount);
}

/// <summary>
/// Draws the specified renderer.
/// </summary>
/// <param name="renderer">The renderer.</param>
/// <param name="point">The point.</param>
/// <param name="context">The context.</param>
/// <param name="flags">The flags.</param>
/// <param name="selBegin">The sel begin.</param>
/// <param name="selEnd">The sel end.</param>
/// <param name="docwport">The doc-viewwport.</param>
/// <returns></returns>
TextDraw CISTextCell::Draw(CISTextRenderer* renderer, fppoint_t point, void* context, TextRenderParam param, uint32_t selBegin, uint32_t selEnd, const TextDocport* docwport) noexcept
{
    assert(renderer);
    // ?? FONT?
    assert(m_pTextFont);
    this->checkLayout();
    // DOCPORT-Y CHECK
    if (docwport) {
        const fp26dot6_t top = point.y + this->position.y;
        const fp26dot6_t bottom = top + m_boxCell.ascent + m_boxCell.descent;
        if (top > docwport->bottom || bottom < docwport->top)
            return {};
    }

    m_pTextRendererWeak = renderer;
    uint32_t count = 0;
    uint32_t drawn = 0;

    const auto fm = m_pTextFont->GetFontMetrics();
    const auto textCount = m_uTextCount;
    const int64_t unit = FONT_METRICS_UNIT;
    const int64_t fsize = m_fpFontSize;
    assert(m_uTextCount <= TEXT_CELL_MAXLEN);
    assert((!m_pInlineObject || m_uTextCount == 1) && "INLINE OBJECT LENGTH => 1");
    // UINT32 WRAPPED
    if (selBegin > selEnd)
        selBegin = 0;

    if (!m_bools.cache) {
        m_bools.cache = true;
        this->clear_cache(renderer);
    }

    const auto env = m_pTextEnv;

    TextEffect tmp;

    for (uint32_t i = 0; i < textCount; ++i) {
        auto& cache = m_aCache[i];
        char16_t ch16 = m_aText[i];
        char32_t ch = ch16;
        int64_t pos = int64_t(m_aPosition[i]) * fsize / unit;
        const int64_t pos2 = int64_t(m_aPosition[i + 1]) * fsize / unit;
        const auto posfp = fp26dot6_t(pos);

        // DOCPORT-X CHECK
        if (docwport) {
            const fp26dot6_t left = point.x + posfp + this->position.x;
            const fp26dot6_t right = left + fp26dot6_t(pos2 - pos);
            if (right < docwport->left || left > docwport->right)
                continue;
        }

        uint32_t number = 1;
        // UTF16 -> UCS4
        if (IsHighSurrogate(ch16)) {
            const auto trail = m_aText[++i];
            assert(IsLowSurrogate(trail));
            ch = UCS4FromChar16x2(ch16, trail);
            number = 2;
        }
        // PASSWORD
        if (m_chPassword)
            ch = m_chPassword;

        const fppoint_t pt{ point.x + posfp + this->position.x, point.y + this->position.y };

        auto effect = m_aEffects + i;
        uint32_t background = effect->background;

        // SELECTION
        if (i >= selBegin && i < selEnd) {
            background = env->selBackground;
            // FOREGROUND OVERRIDE
            if (env->selForeground) {
                tmp = *effect;
                tmp.foreground = env->selForeground;
                effect = &tmp;
            }
        }

        // BACKGROUND
        if (background) {
            const auto ptx = pt.x;
            const auto pty = point.y + this->head;
            const auto ptw = fp26dot6_t(pos2 - pos);
            const auto pth = this->tail - this->head - env->lineHeightDescent;
            renderer->FillRect(background, context, param, { ptx, pty }, { ptw, pth });
        }

        // UNDERLINE
        if (effect->flags & TEXT_EFFECT_FLAG_UNDERLINE) {
            const auto underlineY = fp26dot6_t((unit - fm->underlinePosition) * fsize / unit);
            const auto underlineH = fp26dot6_t(fm->underlineThickness * fsize / unit);
            const auto ptx = pt.x;
            const auto pty = point.y + underlineY;
            const auto ptw = fp26dot6_t(pos2 - pos);
            const auto pth = underlineH;
            renderer->FillRect(effect->foreground, context, param, { ptx, pty }, { ptw, pth });
        }

        // STRIKETHROUGH
        if (effect->flags & TEXT_EFFECT_FLAG_STRIKETHROUGH) {
            const auto strikethroughY = fp26dot6_t((unit - fm->strikethroughPosition) * fsize / unit);
            const auto strikethroughH = fp26dot6_t(fm->underlineThickness * fsize / unit);
            const auto ptx = pt.x;
            const auto pty = point.y + strikethroughY;
            const auto ptw = fp26dot6_t(pos2 - pos);
            const auto pth = strikethroughH;
            renderer->FillRect(effect->foreground, context, param, { ptx, pty }, { ptw, pth });
        }

        // FOREGROUND
        bool rv = true;
        if (m_pInlineObject) {
            rv = renderer->DrawInlineObject(m_pInlineObject, pt, context, param);
        }
        else {
            //const fppoint_t baseline = { pt.x, pt.y - m_boxCell.descent };
            rv = renderer->DrawGlyph(m_pTextFont, *effect, cache, context, param, ch, m_fpFontSize, pt);
        }

        count += number;
        if (rv) {
            drawn += number;
        }
        else {
            this->invalidate_cache();
        }
    }
    return { count, drawn };
}


/// <summary>
/// Draws the line feed.
/// </summary>
/// <param name="renderer">The renderer.</param>
/// <param name="point">The point.</param>
/// <param name="context">The context.</param>
/// <param name="flags">The flags.</param>
/// <param name="docwport">The docwport.</param>
/// <returns></returns>
void CISTextCell::DrawLineFeed(CISTextRenderer* renderer, fppoint_t point, void* context, TextRenderParam param, const TextDocport* docwport) noexcept
{
    const auto background = m_pTextEnv->selBackground;
    const int64_t unit = FONT_METRICS_UNIT;
    const int64_t fsize = m_fpFontSize;
    int64_t pos = int64_t(m_aPosition[m_uTextCount]) * fsize / unit;
    const auto posfp = fp26dot6_t(pos);
    const fppoint_t pt{ point.x + posfp + this->position.x, point.y + this->position.y };
    const auto ptx = pt.x;
    const auto pty = point.y + this->head;
    const auto pth = this->tail - this->head - m_pTextEnv->lineHeightDescent;;
    const auto ptw = pth / 2;
    if (docwport) {
        if (ptx > docwport->right || pty > docwport->bottom)
            return;
        if (ptx + ptw < docwport->left || pty + pth < docwport->top)
            return;
    }
    renderer->FillRect(background, context, param, { ptx, pty }, { ptw, pth });
}



/// <summary>
/// Inserts the text weak.
/// </summary>
/// <param name="pos">The position.</param>
/// <param name="str">The string.</param>
/// <param name="len">The length.</param>
/// <param name="line">The line.</param>
/// <returns></returns>
auto CISTextCell::InsertTextWeak(uint32_t pos, const char16_t * str, uint32_t len, Node * line) noexcept -> WeakInsert
{
    if (len == uint32_t(-1))
        len = Length(str);

    // CENTER
    if (pos < m_uTextCount) {
        const auto back = this->insertTextBackFirst(pos, str, len);
        WeakInsert rv = {};
        rv.front = 0;
        rv.back = back;
        return rv;
    }

    // BACK

    WeakInsert rv = {};
    const auto front = this->insertTextFrontFirst(pos, str, len);

    str += front;
    len -= front;
    rv.front = front;

    if (len && this->next != line) {
        const auto cell = static_cast<CISTextCell*>(this->next);
        assert(cell->m_dbgCode == m_dbgCode);
        rv.back = cell->insertTextBackFirst(0, str, len);
    }
    return rv;
}


/// <summary>
/// Inserts the function move tail effect.
/// </summary>
/// <returns></returns>
void CISTextCell::insertFuncMoveTailEffect(uint32_t count, uint32_t pos) noexcept
{
    const uint32_t tail = m_uTextCount - pos;
    if (tail) {
        std::memmove(m_aText + pos + count, m_aText + pos, sizeof(m_aText[0]) * tail);
        std::memmove(m_aEffects + pos + count, m_aEffects + pos, sizeof(m_aEffects[0]) * tail);
    }
    const TextEffect * target = nullptr;
    // INSERTED
    if (pos) target = m_aEffects + pos - 1;
    // AFTER
    else if (m_uTextCount) target = m_aEffects;
    // DEFAULT
    else target = &m_pTextEnv->defaultEffect;
    // COPY
    for (uint32_t i = 0; i < count; ++i)
        m_aEffects[pos + i] = *target;
}

/// <summary>
/// Inserts the text back first.
/// </summary>
/// <param name="pos">The position.</param>
/// <param name="str">The string.</param>
/// <param name="len">The length.</param>
/// <returns></returns>
uint32_t CISTextCell::insertTextBackFirst(uint32_t pos, const char16_t * const str, uint32_t const len) noexcept
{
    assert(len != uint32_t(-1));
    assert(str < m_aText || str > m_aText + TEXT_CELL_MAXLEN);

    if (m_uTextCount >= TEXT_CELL_MAXLEN)
        return 0;


    uint32_t count = std::min(TEXT_CELL_MAXLEN - m_uTextCount, len);

    if (!count)
        return 0;

    const auto first = str[len - count];
    if (IsLowSurrogate(first))
        --count;

    if (!count)
        return 0;

    // MOVE TAIL/EFFECT
    pos = std::min(pos, m_uTextCount);
    this->insertFuncMoveTailEffect(count, pos);

    std::memcpy(m_aText + pos, str + len - count, sizeof(m_aText[0]) * count);
    m_uTextCount += count;
    this->dirty();
    this->invalidate_cache();
    return count;
}

/// <summary>
/// Inserts the text front first.
/// </summary>
/// <param name="pos">The position.</param>
/// <param name="str">The string.</param>
/// <param name="len">The length.</param>
/// <returns></returns>
uint32_t CISTextCell::insertTextFrontFirst(uint32_t pos, const char16_t * str, uint32_t len) noexcept
{
    assert(len != uint32_t(-1));
    assert(str < m_aText || str > m_aText + TEXT_CELL_MAXLEN);

    if (m_uTextCount >= TEXT_CELL_MAXLEN)
        return 0;

    uint32_t count = std::min(TEXT_CELL_MAXLEN - m_uTextCount, len);

    if (!count)
        return 0;

    const auto last = str[count - 1];
    if (IsHighSurrogate(last))
        --count;

    if (!count)
        return 0;

    // MOVE TAIL/EFFECT
    pos = std::min(pos, m_uTextCount);
    this->insertFuncMoveTailEffect(count, pos);
    
    std::memcpy(m_aText + pos, str, sizeof(char16_t) * count);
    m_uTextCount += count;
    this->dirty();
    this->invalidate_cache();
    return count;
}

/// <summary>
/// Pres the set font.
/// </summary>
/// <param name="size">The size.</param>
/// <param name="face">The face.</param>
/// <returns></returns>
bool CISTextCell::PreSetFont(fp26dot6_t size, IISFontFace* face) const noexcept
{
    bool changed = false;
    if (face && face != m_pTextFont)
        changed = true;
    if (size > 0 && size != m_fpFontSize)
        changed = true;
    return changed;
}

/// <summary>
/// Sets the font.
/// </summary>
/// <param name="">The .</param>
/// <param name="size">The size.</param>
/// <returns></returns>
bool CISTextCell::SetFont(fp26dot6_t size, IISFontFace* face) noexcept
{
    bool changed = false;
    if (face && face != m_pTextFont) {
        CharIS::SafeDispose(m_pTextFont);
        m_pTextFont = face;
        face->AddRefCnt();
        changed = true;
    }
    if (size > 0 && size != m_fpFontSize) {
        m_fpFontSize = size;
        changed = true;
    }
    if (changed) {
        this->dirty();
        this->invalidate_cache();
    }
    return changed;
}

