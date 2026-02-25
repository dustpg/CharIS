#include "is_basic_text_document.h"
#include <charis/include/is_text_document.h>
#include <charis/include/is_text_layout.h>
#include <charis/include/is_base.h>
#include <string>
#include <cassert>
#include <algorithm>
#include <cstdlib>

#include "../renderer/is_text_renderer_impl.h"

using namespace CharIS;


/// <summary>
/// Initializes a new instance of the <see cref="CISTextDocuemnt" /> class.
/// </summary>
/// <param name="manager">The manager.</param>
/// <param name="font">The font.</param>
/// <param name="size">The size.</param>
/// <returns></returns>
BasicLayout::CISTextDocuemnt::CISTextDocuemnt(CISTextRenderer* renderer, IISFontFace* font, fp26dot6_t size) noexcept
    : m_pTextRenderManager(renderer)
    , m_refFontEngine(renderer->RefFontEngine())
    , m_oEngine(font, size)
{
    renderer->AddRefCnt();
    m_sUndoHead.next = &m_sUndoTail;
    m_sUndoTail.prev = &m_sUndoHead;
    m_fpRaiseLowerPoint = -1;
}


/// <summary>
/// Finalizes an instance of the <see cref="CISTextDocuemnt"/> class.
/// </summary>
/// <returns></returns>
BasicLayout::CISTextDocuemnt::~CISTextDocuemnt() noexcept
{
    CharIS::SafeDispose(m_pTextRenderManager);
}


/// <summary>
/// Sets the selection.
/// </summary>
/// <param name="mode">The mode.</param>
/// <param name="pos">The position.</param>
/// <param name="keepAnchor">if set to <c>true</c> [keep anchor].</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetSelection(SELECTION_MODE mode, uint32_t pos, bool keepAnchor) noexcept
{
    if (pos == TEXT_DOC_POSITON_CARET)
        pos = m_uCaret;

    const auto prevAnchor = m_uAnchor;
    const auto prevCaret = m_uCaret;

    // RAISE/LOWER POINT
    this->saveRaisePoint(mode);

    switch (mode)
    {
    case SELECTION_MODE_SET:
        m_uCaret = pos;
        break;
    case SELECTION_MODE_ALL:
        m_uAnchor = 0;
        m_uCaret = m_oEngine.TextLength();
        break;
    case SELECTION_MODE_FRONT:
        m_uCaret = m_oEngine.FrontChar(pos, false);
        break;
    case SELECTION_MODE_BACK:
        m_uCaret = m_oEngine.BackChar(pos, false);
        break;
    case SELECTION_MODE_WORD_FRONT:
        m_uCaret = m_oEngine.FrontChar(pos, true);
        break;
    case SELECTION_MODE_WORD_BACK:
        m_uCaret = m_oEngine.BackChar(pos, true);
        break;
    case SELECTION_MODE_RAISE:
        m_uCaret = m_oEngine.RaiseChar(pos, m_fpRaiseLowerPoint);
        break;
    case SELECTION_MODE_LOWER:
        m_uCaret = m_oEngine.LowerChar(pos, m_fpRaiseLowerPoint);
        break;
    case SELECTION_MODE_HOME:
        m_uCaret = m_oEngine.LineBegin(pos);
        break;
    case SELECTION_MODE_END:
        m_uCaret = m_oEngine.LineEnd(pos);
        break;
    case SELECTION_MODE_FIRST:
        m_uCaret = 0;
        break;
    case SELECTION_MODE_LAST:
        m_uCaret = m_oEngine.TextLength();
        break;
    case SELECTION_MODE_RAISE_VIEW:
        break;
    case SELECTION_MODE_LOWER_VIEW:
        break;
    case SELECTION_MODE_RAISE_SHIFT:
        break;
    case SELECTION_MODE_LOWER_SHIFT:
        break;
    }

    // DONOT KEEP
    if (!keepAnchor) 
        m_uAnchor = m_uCaret;

    // CHANGED
    if (m_uCaret != prevCaret || m_uAnchor != prevAnchor) {

        // SELECTION AREA
        if (m_uCaret != m_uAnchor) {
            const auto begin = std::min(m_uCaret, m_uAnchor);
            const auto end = std::max(m_uCaret, m_uAnchor);
            m_oEngine.SetSelectionBlock({ begin, end - begin });
        }
        else
            m_oEngine.SetSelectionBlock({ });

        return CODE_OK;
    }

    return CODE_FALSE;
}



/// <summary>
/// Saves the raise point.
/// </summary>
/// <param name="mode">The mode.</param>
/// <returns></returns>
void BasicLayout::CISTextDocuemnt::saveRaisePoint(SELECTION_MODE mode) noexcept
{
    if (mode == SELECTION_MODE_RAISE || mode == SELECTION_MODE_LOWER) {
        if (m_fpRaiseLowerPoint == 0) {
            HitTestCtx ctx;
            this->HitTest(m_uCaret, ctx, TEXT_COORDINATE_SYSTEM_DOCUMENT);
            m_fpRaiseLowerPoint = ctx.caret1.x;
        }
    }
    else {
        m_fpRaiseLowerPoint = 0;
    }
}

/// <summary>
/// Gets the selection.
/// </summary>
/// <param name="sel">The sel.</param>
/// <param name="count">The count.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::GetSelection(TextSelection sel[], uint32_t & count) noexcept
{
    const auto input = count;
    count = 1;
    if (input < 1) 
        return CODE_SMALLBUFFER;
    sel[0].anchor = m_uAnchor;
    sel[0].caret = m_uCaret;
    return CODE_OK;
}

/// <summary>
/// Converts text line terminator enum to LineFeed structure.
/// </summary>
/// <param name="t">The terminator type.</param>
/// <returns></returns>
static inline BasicLayout::LineFeed text_line_terminator(TEXT_LINE_TERMINATOR t) noexcept {
    BasicLayout::LineFeed lf;
    switch (t)
    {
    default:
    case TEXT_LINE_TERMINATOR_LF:
        lf.newline[0] = '\n';
        lf.newline[1] = 0;
        lf.nlcount = 1;
        break;
    case TEXT_LINE_TERMINATOR_CRLF:
        lf.newline[0] = '\r';
        lf.newline[1] = '\n';
        lf.nlcount = 2;
        break;
    case TEXT_LINE_TERMINATOR_CR:
        lf.newline[0] = '\r';
        lf.newline[1] = 0;
        lf.nlcount = 1;
        break;
    }
    return lf;
}

/// <summary>
/// Sets the attribute.
/// </summary>
/// <param name="attr">The attribute.</param>
/// <param name="value">The value.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetAttribute(TEXT_DOC_ATTR attr, int32_t value) noexcept
{
    switch (attr)
    {
        CODE code;
    case TEXT_DOC_ATTR_LINE_TERMINATOR:
        return m_oEngine.SetLineFeed(text_line_terminator(TEXT_LINE_TERMINATOR(value)));
    case TEXT_DOC_ATTR_RICH_TEXT:
        m_bRichText = !!value;
        return CODE_OK;
    case TEXT_DOC_ATTR_PASSWORD_CHAR32:
        m_oEngine.SetPasswordMode(char32_t(value));
        return CODE_OK;
    case TEXT_DOC_ATTR_KERNING:
        m_oEngine.SetKerning(!!value);
        return CODE_OK;
    case TEXT_DOC_ATTR_SINGLE_LINE:
        m_oEngine.SetSingleLine(!!value);
        return CODE_OK;
    case TEXT_DOC_ATTR_SELECTION_FOREGROUND:
        m_oEngine.SetSelectionForeground(uint32_t(value));
        return CODE_OK;
    case TEXT_DOC_ATTR_SELECTION_BACKGROUND:
        m_oEngine.SetSelectionBackground(uint32_t(value));
        return CODE_OK;
    case TEXT_DOC_ATTR_LINE_HEIGHT_ASCENT:
        m_oEngine.SetLineHeightAscent(value);
        return CODE_OK;
    case TEXT_DOC_ATTR_LINE_HEIGHT_DESCENT:
        m_oEngine.SetLineHeightDescent(value);
        return CODE_OK;
    case TEXT_DOC_ATTR_WRAP_MODE:
        code = m_oEngine.SetWrapMode(WRAP_MODE(value));
        m_oEngine.Shrink();
        return code;
    case TEXT_DOC_ATTR_TEXT_ALIGN:
        return  m_oEngine.SetTextAlign(TEXT_ALIGN(value));
    case TEXT_DOC_ATTR_VERTICAL_ALIGN:
        return  m_oEngine.SetVerticalAlign(VERTICAL_ALIGN(value));
    case TEXT_DOC_ATTR_PARAGRAPH_HEIGHT_ASCENT:
        break;
    case TEXT_DOC_ATTR_PARAGRAPH_HEIGHT_DESCENT:
        break;
    }
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

/// <summary>
/// Begins the op.
/// </summary>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::BeginOp() noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

/// <summary>
/// Ends the op.
/// </summary>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::EndOp() noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

/// <summary>
/// Undoes this instance.
/// </summary>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::Undo() noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

/// <summary>
/// Redoes this instance.
/// </summary>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::Redo() noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}


/// <summary>
/// Sets the undo limit.
/// </summary>
/// <param name="limit">The limit.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetUndoLimit(uint32_t limit) noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

/// <summary>
/// Clears the undo.
/// </summary>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::ClearUndo() noexcept
{
    if (!m_uUndoCount)
        return CODE_FALSE;

    auto node = m_sUndoHead.next;

    while (node != &m_sUndoTail) {
        // TODO: Implement proper command structure for undo/redo
        // For now, just free the node memory
        auto next = node->next;
        std::free(node);
        node = next;
    }
    m_uUndoCount = 0;

    return CODE_OK;
}

/// <summary>
/// Serializes the specified .
/// </summary>
/// <param name="blob">The BLOB.</param>
/// <param name="format">The format.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::Serialize(IISBlob * blob, const char * format) noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}


/// <summary>
/// Deserializes the specified .
/// </summary>
/// <param name="blob">The BLOB.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::Deserialize(IISBlob * blob) noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}


/// <summary>
/// Draws this instance.
/// </summary>
/// <param name="context">The context.</param>
/// <param name="flags">The flags.</param>
/// <param name="range">The range.</param>
/// <returns></returns>
TextDraw BasicLayout::CISTextDocuemnt::Draw(void* context, TextRenderParam param, const TextViewportRange* range) noexcept
{
    return m_oEngine.Draw(m_pTextRenderManager, context, param, range);
}


/// <summary>
/// Inserts the text.
/// </summary>
/// <param name="position">The position.</param>
/// <param name="text">The text.</param>
/// <param name="len">The length.</param>
/// <param name="inserted">The inserted count.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::InsertText(uint32_t position, const char16_t * text, uint32_t len, uint32_t* inserted) noexcept
{
    const auto len1 = m_oEngine.TextLength();
    const auto code = m_oEngine.InsertText(position, text, len);
    if (inserted) {
        *inserted = m_oEngine.TextLength() - len1;
    }
    return code;
}



/// <summary>
/// Sets the direction.
/// </summary>
/// <param name="reading">The reading.</param>
/// <param name="paragraph">The paragraph.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetDirection(TEXT_DIRECTION reading, TEXT_DIRECTION paragraph) noexcept
{
    const auto r = uint32_t(reading) & 1;
    const auto p = uint32_t(paragraph) & 1;
    if (r == p) {
        // TODO: Add proper logging mechanism
        // TS_LOG_ERROR("[setDirection] orthogonal!\n");
        return CODE_INVALIDARG;
    }
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

/// <summary>
/// Gets the size of the layout.
/// </summary>
/// <param name="size">The size.</param>
/// <param name="cs">The cs.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::GetLayoutSize(fpsize_t & size, TEXT_COORDINATE_SYSTEM cs) noexcept
{
    size = m_oEngine.DocSize();
    return CODE_OK;
}

/// <summary>
/// Removes the text.
/// </summary>
/// <param name="range">The range.</param>
/// <param name="removed">The removed count.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::RemoveText(Range range, uint32_t* removed) noexcept
{
    const auto len = m_oEngine.TextLength();
    const auto code = m_oEngine.RemoveText(range);
    if (removed) {
        *removed = len - m_oEngine.TextLength();
    }
    return code;
}

/// <summary>
/// Sets the size of the font.
/// </summary>
/// <param name="range">The range.</param>
/// <param name="size">The size.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetFontSize(Range range, fp26dot6_t size) noexcept
{
    if (!m_bRichText)
        return CODE_FAILED;

    return m_oEngine.SetFont(range, nullptr, size);
}


/// <summary>
/// Sets the font face.
/// </summary>
/// <param name="range">The range.</param>
/// <param name="name">The name.</param>
/// <param name="w">The w.</param>
/// <param name="se">The se.</param>
/// <param name="sh">The sh.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetFontFace(Range range, const char16_t * name, FONT_WEIGHT w, FONT_STYLE se, FONT_STRETCH sh) noexcept
{
    if (!m_bRichText)
        return CODE_FAILED;
    IISFontFace* face = nullptr;
    // MATCH
    CODE code = m_pTextRenderManager->RefFontEngine().MatchFontFace(&face, name, w, se, sh);
    // SET
    if (Success(code)) {
        code = m_oEngine.SetFont(range, face);
    }
    CharIS::SafeDispose(face);
    return code;
}

/// <summary>
/// Sets the sub effect.
/// </summary>
/// <param name="range">The range.</param>
/// <param name="offset">The offset.</param>
/// <param name="value">The value.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetSubEffect(Range range, uint32_t offset, uint32_t value) noexcept
{
    if (!m_bRichText)
        return CODE_FAILED;
    return m_oEngine.SetSubEffect(range, offset, value);
}


/// <summary>
/// Sets the effect flag.
/// </summary>
/// <param name="range">The range.</param>
/// <param name="bitIndex">The bit index.</param>
/// <param name="on">if set to <c>true</c> [on].</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetEffectFlag(Range range, uint32_t bitIndex, bool on) noexcept
{
    if (!m_bRichText)
        return CODE_FAILED;
    return m_oEngine.SetEffectFlag(range, bitIndex, on);
}

/// <summary>
/// Makes the plain string.
/// </summary>
/// <param name="range">The range.</param>
/// <param name="string">The string.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::MakePlainString(Range range, IISStringU16 * string) noexcept
{
    if (!string)
        return CODE_POINTER;
    return m_oEngine.MakeString(range, *string);
}


/// <summary>
/// Sets the defualt effect.
/// </summary>
/// <param name="effect">The effect.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetDefualtEffect(const TextEffect & effect) noexcept
{
    m_oEngine.SetDefaultEffect(effect);
    return CODE_OK;
}

/// <summary>
/// Sets the inline object.
/// </summary>
/// <param name="range">The range.</param>
/// <param name="obj">The object.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetInlineObject(Range range, IISInlineObject * obj) noexcept
{
    return m_oEngine.SetInlineObject(range, obj);
}

/// <summary>
/// Sets the size of the canvas.
/// </summary>
/// <param name="size">The size.</param>
/// <param name="cs">The cs.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetCanvasSize(fpsize_t size, TEXT_COORDINATE_SYSTEM cs) noexcept
{
    m_oEngine.SetCanvasSize(size);
    return CODE_OK;
}


/// <summary>
/// Sets the viewpoint.
/// </summary>
/// <param name="pos">The position.</param>
/// <param name="cs">The cs.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::SetViewpoint(fppoint_t pos, TEXT_COORDINATE_SYSTEM cs) noexcept
{
    assert(!"NOTIMPL");
    return CODE_NOTIMPL;
}

/// <summary>
/// Hits the test.
/// </summary>
/// <param name="point">The point.</param>
/// <param name="ctx">The CTX.</param>
/// <param name="cs">The cs.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::HitTest(fppoint_t point, HitTestCtx & ctx, TEXT_COORDINATE_SYSTEM cs) noexcept
{
    m_oEngine.HitTest(point, ctx);
    return CODE_OK;
}

/// <summary>
/// Hits the test.
/// </summary>
/// <param name="position">The position.</param>
/// <param name="ctx">The CTX.</param>
/// <param name="cs">The cs.</param>
/// <returns></returns>
CODE BasicLayout::CISTextDocuemnt::HitTest(uint32_t position, HitTestCtx & ctx, TEXT_COORDINATE_SYSTEM cs) noexcept
{
    m_oEngine.HitTest(position, ctx);
    return CODE_OK;
}

extern "C"
/// <summary>
/// Creates a base text document.
/// </summary>
/// <param name="layout">The output document.</param>
/// <param name="manager">The font manager.</param>
/// <param name="defaultFontName">Default name of the font.</param>
/// <param name="defaultFontSize">Default size of the font.</param>
/// <returns></returns>
CharIS::CODE CharisCreateTextDocument(
    CharIS::IISTextDocument** document,
    CharIS::IISTextRenderer* renderer,
    const char16_t* defaultFontName, 
    CharIS::fp26dot6_t defaultFontSize,
    int flag
) noexcept
{
    assert(document);
    if (!renderer || !document)
        return CharIS::CODE_POINTER;

    const auto textRenderer = static_cast<CISTextRenderer*>(renderer);

    CharIS::CODE code = CharIS::CODE_OK;
    CharIS::IISFontFace* face = nullptr;
    CharIS::BasicLayout::CISTextDocuemnt* doc = nullptr;
    // FIND FONT
    if (CharIS::Success(code)) {
        code = textRenderer->RefFontEngine().MatchFontFace(&face, defaultFontName);
    }
    // FIND DEFAULT FONT
    if (CharIS::Success(code)) {
        doc = new (std::nothrow) CharIS::BasicLayout::CISTextDocuemnt{ textRenderer , face, defaultFontSize };
        if (!doc)
            code = CharIS::CODE_OUTOFMEMORY;
    }
    // TAKE THIS
    if (CharIS::Success(code)) {
        *document = CharIS::Take(doc);
    }
    CharIS::SafeDispose(face);
    CharIS::SafeDispose(doc);
    return code;
}
