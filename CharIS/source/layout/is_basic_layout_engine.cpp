#include "is_basic_layout_engine.h"
#include "is_text_cell.h"
#include "../utils/is_unicode.h"
#include <CharIS/include/is_base.h>
#include <CharIS/include/is_blob_string.h>
#include <CharIS/include/is_font_face.h>
#include <cassert>
#include <algorithm>
#include <cstring>

namespace CharIS { namespace BasicLayout {

    // TODO: RENAME THIS
    static BaseCellBox boxCast(const BaseCellBox& box, VERTICAL_ALIGN align) noexcept {
        BaseCellBox ret{};
        switch (align) {
        case VERTICAL_ALIGN_ASCENDER:
            ret.advance = box.advance;
            ret.ascent = 0;
            ret.descent = box.ascent + box.descent;
            break;
        case VERTICAL_ALIGN_MIDDLE:
            ret.advance = box.advance;
            ret.descent = ret.ascent = (box.ascent + box.descent) / 2;
            break;
        case VERTICAL_ALIGN_DESCENDER:
            ret.advance = box.advance;
            ret.ascent = box.ascent + box.descent;
            ret.descent = 0;
            break;
        default:
            ret = box;
        }
        return ret;
    }

}}

using namespace CharIS;

BasicLayout::CISLayoutEngine::CISLayoutEngine(IISFontFace* face, fp26dot6_t size) noexcept
    : m_pDefaultFont(face)
    , m_fpDefaultSize(size)
{
    assert(face);
    face->AddRefCnt();

    m_sLineFeed = {};
#ifdef _WIN32
    m_sLineFeed.newline[0] = '\r';
    m_sLineFeed.newline[1] = '\n';
    m_sLineFeed.nlcount = 2;
#else
    m_sLineFeed.newline[0] = '\n';
    m_sLineFeed.newline[1] = 0;
    m_sLineFeed.nlcount = 1;
#endif

    m_sHead.prev = nullptr;
    m_sHead.next = &m_sTail;

    m_sTail.prev = &m_sHead;
    m_sTail.next = nullptr;
#ifndef NDEBUG
    std::memset(dbg_buffer, 0x99, sizeof(dbg_buffer));
#endif
    m_sEnv.defaultEffect.foreground = uint32_t(-1);
    m_sEnv.defaultEffect.foreground = uint32_t(0xff000000);
    m_sEnv.selBackground = 0xffffcc66;
}

CODE BasicLayout::CISLayoutEngine::SetLineFeed(LineFeed fmt) noexcept
{
    if (fmt.nlcount != 1 && fmt.nlcount != 2)
        return CODE_INVALIDARG;
    if (fmt.newline[0] != '\r' && fmt.newline[0] != '\n')
        return CODE_INVALIDARG;

    LineFeed env = m_sLineFeed;
    if (env.nlcount == fmt.nlcount &&
        !std::memcmp(env.newline, fmt.newline, sizeof(char16_t) * env.nlcount))
        return CODE_FALSE;

    const auto line = int32_t(m_vParagraphs.size());

    if (line > 1 && m_cTextLength) {
        const auto newlen = int32_t(m_cTextLength) + (int32_t(fmt.nlcount) - int32_t(m_sLineFeed.nlcount)) * (line - 2);
        assert(newlen > 0);
        if (newlen < 0)
            return CODE_UNEXPECTED;
        m_cTextLength = uint32_t(newlen);
    }

    m_sLineFeed = fmt;
    return CODE_OK;
}

BasicLayout::CISLayoutEngine::~CISLayoutEngine() noexcept
{
    this->removeAll();
    CharIS::SafeDispose(m_pDefaultFont);
}

bool BasicLayout::CISLayoutEngine::doShrink() noexcept
{
    if (m_sSetting.wrapMode == WRAP_MODE_NO_WRAP || m_sSetting.canvasSize.width <= 0) {
        if (m_vParagraphs.size() < 1)
            return false;

        const auto beginPar = m_vParagraphs.cbegin();
        const auto endPar = m_vParagraphs.cend() - 1;

        for (auto itr = beginPar; itr != endPar; ++itr) {
            const auto& paragraph = *itr;
            const auto begin = paragraph.first;
            const auto end = (itr + 1)->first;
            uint32_t remain = 0;
            for (Node* node = begin; node != end; node = node->next) {
                const auto cell = static_cast<CISTextCell*>(node);
                const auto len = static_cast<uint32_t>(cell->GetView().len);
                if (!cell->SameNext(end)) {
                    remain = 0;
                    continue;
                }
                remain += len;
                if (remain >= TEXT_CELL_MAXLEN) {
                    const uint32_t pos = TEXT_CELL_MAXLEN + len - remain;
                    remain -= TEXT_CELL_MAXLEN;
                    cell->SplitWeak(pos);
                }
            }
            for (Node* node = begin; node != end; node = node->next) {
                const auto cell = static_cast<CISTextCell*>(node);
                cell->MergeNext(end);
            }
        }
        return true;
    }
    CHARIS_LOGGER_ERR("%s: Currently, the shrink operation is only supported in the NO_WRAP mode.", __FUNCTION__);
    return false;
}

void BasicLayout::CISLayoutEngine::HitTest(uint32_t pos, HitTestCtx& output) noexcept
{
    output = {};
    this->checkLayout();
    FindCtx ctx;
    if (!this->findPos(pos, ctx))
        return;

    const auto end = ctx.par;

    fp26dot6_t offsetY = 0;

    for (auto itr = m_vParagraphs.data(); itr < end; ++itr) {
        offsetY += itr->size.height;
    }

    ctx.cell->HitTest(ctx.offsetInCell, output);
    output.caret1.y += offsetY - m_sEnv.lineHeightAscent;
    output.caret2.y += offsetY;
    output.postion = pos;
}

void BasicLayout::CISLayoutEngine::HitTest(fppoint_t pos, HitTestCtx& output) noexcept
{
    output = {};
    this->checkLayout();
    pos.x += m_ptOrigin.x;
    pos.y += m_ptOrigin.y;

    auto lineY = pos.y;

    if (m_vParagraphs.size() < 2)
        return;

    fp26dot6_t offsetY = {};
    uint32_t count = 0;

    Paragraph* p = nullptr;
    for (auto& par : m_vParagraphs) {
        p = &par;
        const auto height = par.size.height;
        if (lineY <= height)
            break;
        lineY -= height;
        count += static_cast<uint32_t>(p->length + m_sLineFeed.nlcount);
        offsetY += p->size.height;
    }
    assert(p);
    if (p->first == &m_sTail) {
        --p;
        offsetY -= p->size.height;
        assert(m_sTail.prev != &m_sHead);
        if (m_sTail.prev != &m_sHead) {
            const auto cell = static_cast<CISTextCell*>(m_sTail.prev);
            cell->HitTest(cell->RefBox().advance, output);
            output.caret1.y += offsetY - m_sEnv.lineHeightAscent;
            output.caret2.y += offsetY;
            output.postion = m_cTextLength;
            return;
        }
        count -= static_cast<uint32_t>(p->length);
    }

    const auto begin = p[0].first;
    const auto end = p[1].first;
    const auto last = static_cast<CISTextCell*>(end->prev)->tail;
    lineY = std::min(std::max(lineY, fp26dot6_t(0)), last);

    CISTextCell* lineBegin = begin;

    for (Node* node = begin; node != end; node = static_cast<CISTextCell*>(node->next)) {
        if (lineY <= static_cast<CISTextCell*>(node)->tail) {
            lineBegin = static_cast<CISTextCell*>(node);
            break;
        }
        count += static_cast<uint32_t>(static_cast<CISTextCell*>(node)->GetView().len);
    }
    const auto head = lineBegin->head;
    CISTextCell* lineEnd = static_cast<CISTextCell*>(end);

    for (Node* node = lineBegin; node != end; node = static_cast<CISTextCell*>(node->next)) {
        if (static_cast<CISTextCell*>(node)->head != head) {
            lineEnd = static_cast<CISTextCell*>(node);
            break;
        }
    }
    assert(lineBegin != lineEnd);

    const auto front = lineBegin->position.x;
    const auto cell = static_cast<CISTextCell*>(lineEnd->prev);
    const auto back = cell->position.x + cell->RefBox().advance;
    const auto lineX = std::min(std::max(pos.x, front), back);
    for (Node* node = lineBegin; node != lineEnd; node = static_cast<CISTextCell*>(node->next)) {
        const auto c = static_cast<CISTextCell*>(node);
        const auto x2 = c->RefBox().advance + c->position.x;
        if (lineX <= x2) {
            c->HitTest(lineX - c->position.x, output);
            output.caret1.y += offsetY - m_sEnv.lineHeightAscent;
            output.caret2.y += offsetY;
            output.postion += count;
            return;
        }
        count += static_cast<uint32_t>(c->GetView().len);
    }
}

void BasicLayout::CISLayoutEngine::SetCanvasOrigin(fppoint_t) noexcept
{
    assert(!"NOTIMPL");
}

void BasicLayout::CISLayoutEngine::Shrink() noexcept
{
    m_bNeedShrink = true;
}

fpsize_t BasicLayout::CISLayoutEngine::DocSize() noexcept
{
    this->checkLayout();
    return m_fsDocSize;
}

void BasicLayout::CISLayoutEngine::removeAll() noexcept
{
    auto node = m_sHead.next;
    while (node != &m_sTail) {
        const auto cell = static_cast<CISTextCell*>(node);
        node = node->next;
        cell->Remove();
        delete cell;
    }
    m_vParagraphs.clear();
    m_bDirty = true;
    m_cTextLength = 0;
}

void BasicLayout::CISLayoutEngine::SetDefaultEffect(const TextEffect& effect) noexcept
{
    m_sEnv.defaultEffect = effect;
}

CODE BasicLayout::CISLayoutEngine::SetFont(Range range, IISFontFace* face, fp26dot6_t size) noexcept
{
    if (!face && size <= 0)
        return CODE_FAILED;

    if (range.length >= m_cTextLength)
        range.length = m_cTextLength - range.position;

    FindCtx front, back;
    const auto r1 = this->findPos(range.position, front);
    const auto r2 = this->findPos(range.position + range.length, back);
    if (!r1 || !r2)
        return CODE_FALSE;

    if (front.cell == back.cell) {
        if (front.offsetInCell == back.offsetInCell)
            return CODE_FALSE;
        if (!front.cell->PreSetFont(size, face))
            return CODE_FALSE;

        front.cell->SplitWeak(back.offsetInCell);
        const auto c1 = front.cell->SplitWeak(front.offsetInCell);
        if (!c1)
            return CODE_OUTOFMEMORY;
        c1->SetFont(size, face);
        front.par->dirty = true;
        m_bDirty = true;
    }
    else {
        if (front.offsetInCell < static_cast<uint32_t>(front.cell->GetView().len) && front.cell->PreSetFont(size, face)) {
            const auto c1 = front.cell->SplitWeak(front.offsetInCell);
            if (!c1)
                return CODE_OUTOFMEMORY;
            c1->SetFont(size, face);
        }
        if (back.offsetInCell && back.cell->PreSetFont(size, face)) {
            const auto c2 = back.cell->SplitWeak(back.offsetInCell);
            if (!c2)
                return CODE_OUTOFMEMORY;
            back.cell->SetFont(size, face);
        }
        auto node = front.cell->next;
        while (node != back.cell) {
            const auto cell = static_cast<CISTextCell*>(node);
            cell->SetFont(size, face);
            node = node->next;
        }
        std::for_each(front.par, back.par + 1, [](Paragraph& par) noexcept {
            par.dirty = true;
        });
        m_bDirty = true;
    }
    return CODE_OK;
}

CODE BasicLayout::CISLayoutEngine::RemoveText(Range range) noexcept
{
    if (range.length >= m_cTextLength)
        range.length = m_cTextLength - range.position;
    FindCtx front, back;
    const auto r1 = this->findPos(range.position, front);
    const auto r2 = this->findPos(range.position + range.length, back);
    if (!r1 || !r2)
        return CODE_FALSE;

    if (front.cell == back.cell) {
        if (front.offsetInCell == back.offsetInCell)
            return CODE_FALSE;
        const auto length = back.offsetInCell - front.offsetInCell;
        assert(length == range.length);
        front.cell->RemoveText({ front.offsetInCell, length });
        assert(m_cTextLength >= length && front.par->length >= length);
        m_cTextLength -= length;
        front.par->length -= length;
        front.par->dirty = true;
        m_bDirty = true;
    }
    else {
        uint32_t length;
        length = static_cast<uint32_t>(front.cell->GetView().len) - front.offsetInCell;
        front.cell->RemoveText({ front.offsetInCell, length });
        length += back.offsetInCell;
        back.cell->RemoveText({ 0, back.offsetInCell });
        auto node = front.cell->next;
        while (node != back.cell) {
            const auto cell = static_cast<CISTextCell*>(node);
            node = node->next;
            length += static_cast<uint32_t>(cell->GetView().len);
            cell->Remove();
            delete cell;
        }
        if (front.par == back.par) {
            assert(back.offsetInPar >= front.offsetInPar);
            assert(front.par->length >= length);
            assert(length == range.length);
            front.par->length -= length;
            front.par->dirty = true;
        }
        else {
            assert(back.par > front.par);
            assert(back.par->length >= back.offsetInPar);
            const uint32_t len1 = front.offsetInPar;
            const uint32_t len2 = back.par->length - back.offsetInPar;
            front.par->dirty = true;
            front.par->length = len1 + len2;
            {
                const auto index0 = front.par - m_vParagraphs.data();
                const auto index1 = back.par - m_vParagraphs.data();
                const auto itr0 = m_vParagraphs.begin() + index0 + 1;
                const auto itr1 = m_vParagraphs.begin() + index1 + 1;
                m_vParagraphs.erase(itr0, itr1);
                length += m_sLineFeed.nlcount * uint32_t(index1 - index0);
            }
            assert(uint32_t(length - range.length) < m_sLineFeed.nlcount);
        }
        front.cell->MergeNext(front.par[1].first);
        assert(m_cTextLength >= length);
        m_cTextLength -= length;
        m_bDirty = true;
    }
    return CODE_OK;
}

CODE BasicLayout::CISLayoutEngine::MakeString(Range range, IISStringU16& str) noexcept
{
    if (range.position || range.length < m_cTextLength)
        return this->makeRangedString(range, str);

    CODE code = CODE_OK;
    auto node = m_sHead.next;

    if (m_vParagraphs.empty())
        return CODE_FALSE;

    auto paragraph = &m_vParagraphs.front();
    ++paragraph;

    while (node != &m_sTail) {
        const auto cell = static_cast<CISTextCell*>(node);
        node = node->next;
        code = cell->AppendString(str);
        if (Failure(code))
            return code;
        if (node != &m_sTail && node == paragraph->first) {
            ++paragraph;
            code = str.Append({ m_sLineFeed.newline, m_sLineFeed.nlcount });
            if (Failure(code))
                return code;
        }
    }
    assert(str.GetView().len == m_cTextLength);
    return code;
}

CODE BasicLayout::CISLayoutEngine::makeRangedString(Range range, IISStringU16& str) noexcept
{
    FindCtx ctx0, ctx1;
    if (!this->findPos(range.position, ctx0))
        return CODE_FALSE;
    if (!this->findPos(range.position + range.length, ctx1))
        return CODE_FALSE;

    if (ctx0.cell == ctx1.cell) {
        auto view = ctx0.cell->GetView();
        view.ptr += ctx0.offsetInCell;
        view.len = ctx1.offsetInCell - ctx0.offsetInCell;
        return str.Append(view);
    }

    auto paragraph = ctx0.par + 1;
    const auto end = ctx1.cell;
    Node* node = ctx0.cell;
    CODE code = CODE_OK;

    auto offset = ctx0.offsetInCell;
    while (node != end) {
        const auto cell = static_cast<CISTextCell*>(node);
        node = node->next;
        auto view = cell->GetView();
        view.ptr += offset;
        view.len -= offset;
        offset = 0;
        code = str.Append(view);
        if (Failure(code))
            return code;
        if (node == paragraph->first) {
            ++paragraph;
            code = str.Append({ m_sLineFeed.newline, m_sLineFeed.nlcount });
            if (Failure(code))
                return code;
        }
    }

    auto view = end->GetView();
    view.len = ctx1.offsetInCell;
    code = str.Append(view);
    return code;
}

template<typename T>
CODE BasicLayout::CISLayoutEngine::effect(Range range, T func) noexcept
{
    FindCtx ctx0, ctx1;
    if (!this->findPos(range.position, ctx0))
        return CODE_FALSE;
    if (!this->findPos(range.position + range.length, ctx1))
        return CODE_FALSE;
    if (ctx0.cell == ctx1.cell) {
        func(ctx0.cell, ctx0.offsetInCell, ctx1.offsetInCell);
    }
    else {
        CISTextCell* node = ctx0.cell;
        func(node, ctx0.offsetInCell, static_cast<uint32_t>(node->GetView().len));
        node = static_cast<CISTextCell*>(node->next);
        const auto end = ctx1.cell;
        while (node != end) {
            func(node, 0, static_cast<uint32_t>(node->GetView().len));
            node = static_cast<CISTextCell*>(node->next);
        }
        func(end, 0, ctx1.offsetInCell);
    }
    return CODE_OK;
}

CODE BasicLayout::CISLayoutEngine::SetSubEffect(Range range, uint32_t offset, uint32_t value) noexcept
{
    if (offset > sizeof(TextEffect) - sizeof(uint32_t)) {
        assert(!"BAD OFFSET");
        return CODE_INVALIDARG;
    }
    return this->effect(range, [=](CISTextCell* cell, uint32_t begin, uint32_t end) noexcept {
        cell->SetSubEffect(begin, end, offset, value);
    });
}

CODE BasicLayout::CISLayoutEngine::SetEffectFlag(Range range, uint32_t bitNum, bool on) noexcept
{
    assert(bitNum < 32);
    if (on) {
        return this->effect(range, [=](CISTextCell* cell, uint32_t begin, uint32_t end) noexcept {
            cell->SetEffectFlagOn(begin, end, bitNum);
        });
    }
    return this->effect(range, [=](CISTextCell* cell, uint32_t begin, uint32_t end) noexcept {
        cell->SetEffectFlagOff(begin, end, bitNum);
    });
}

CODE BasicLayout::CISLayoutEngine::SetInlineObject(Range range, IISInlineObject* obj) noexcept
{
    if (range.length != 1) {
        CHARIS_LOGGER_ERR("[%s] unsupported yet(range.length != 1)", __FUNCTION__);
        return CODE_NOTIMPL;
    }

    FindCtx ctx;
    const auto ret = this->findPos(range.position, ctx);
    if (!ret)
        return CODE_FALSE;
    const auto cell1 = ctx.cell->SplitWeak(ctx.offsetInCell);
    if (!cell1)
        return CODE_OUTOFMEMORY;
    const auto cell2 = cell1->SplitWeak(1);
    if (!cell2)
        return CODE_OUTOFMEMORY;

    cell1->SetInlineObject(obj);
    ctx.par->dirty = true;
    m_bDirty = true;
    return CODE_OK;
}

CODE BasicLayout::CISLayoutEngine::InsertText(uint32_t pos, const char16_t* str, uint32_t len) noexcept
{
    if (len == uint32_t(-1))
        len = CharIS::Length(str);

    uint32_t last = len;

    for (uint32_t i = 0; i != len; ++i) {
        uint32_t index = len - i - 1;

        if (str[index] == '\r')
            --last;

        if (str[index] == '\n') {
            auto code = this->insertLine(pos, str + index + 1, last - index - 1);
            last = index;
            if (Failure(code))
                return code;
            if (!m_bSingleLine) {
                code = this->newLine(pos);
                if (Failure(code))
                    return code;
            }
        }
    }

    return this->insertLine(pos, str, last);
}

CODE BasicLayout::CISLayoutEngine::insertLine(uint32_t pos, const char16_t* str, uint32_t len) noexcept
{
    if (!len)
        return CODE_OK;
    assert(len != uint32_t(-1));

    FindCtx ctx;

    if (!this->findPos(pos, ctx))
        return CODE_OUTOFMEMORY;

    auto cell = ctx.cell;
    const auto paragraph = ctx.par;

    Node* const nextLine = paragraph[1].first;

    uint32_t posInCell = ctx.offsetInCell;
    {
        const auto weak = cell->InsertTextWeak(posInCell, str, len, nextLine);
        const uint32_t all = weak.front + weak.back;
        if (all) {
            str += weak.front;
            posInCell += weak.front;
            len -= all;
            paragraph->length += all;
            m_cTextLength += all;
            m_bDirty = true;
            paragraph->dirty = true;
        }
    }

    if (!len)
        return CODE_OK;

    const auto next = cell->SplitStrong(posInCell);
    if (!next)
        return CODE_OUTOFMEMORY;

    {
        const auto weak = cell->InsertTextWeak(posInCell, str, len, nextLine);
        const uint32_t all = weak.front + weak.back;
        if (all) {
            str += weak.front;
            posInCell += weak.front;
            len -= all;
            paragraph->length += all;
            m_cTextLength += all;
            m_bDirty = true;
            paragraph->dirty = true;
        }
    }

    while (len) {
        const auto alloc = new(std::nothrow) CISTextCell{ &cell->RefFontFace(), &m_sEnv, cell->GetFontSize() };
        if (!alloc)
            return CODE_OUTOFMEMORY;
        const auto count = alloc->AppendText(str, len);
        str += count;
        len -= count;
        paragraph->length += count;
        m_cTextLength += count;
        m_bDirty = true;
        paragraph->dirty = true;
        cell->Insert(alloc);
        cell = alloc;
    }

    const auto last = paragraph[1].first;
    if (next != last)
        next->MergeNext(last);

    return CODE_OK;
}

bool BasicLayout::CISLayoutEngine::baseParagraph() noexcept
{
    if (!m_vParagraphs.empty())
        return true;

    const auto obj = new(std::nothrow) CISTextCell{ m_pDefaultFont, &m_sEnv, m_fpDefaultSize };
    if (!obj)
        return false;

    try {
        m_vParagraphs.emplace_back().first = obj;
        m_vParagraphs.emplace_back().first = reinterpret_cast<CISTextCell*>(&m_sTail);
    }
    catch (...) {
        delete obj;
        return false;
    }

    m_sHead.Insert(obj);
    return true;
}

CODE BasicLayout::CISLayoutEngine::newLine(uint32_t pos) noexcept
{
    FindCtx ctx;
    if (!this->findPos(pos, ctx))
        return CODE_OUTOFMEMORY;

    auto posInCell = ctx.offsetInCell;
    const auto next = ctx.cell->SplitStrong(posInCell);
    if (!next)
        return CODE_OUTOFMEMORY;

    Paragraph nextParagraph = {};
    nextParagraph.dirty = true;
    nextParagraph.first = next;
    nextParagraph.length = ctx.par->length - ctx.offsetInPar;

    if (ctx.offsetInPar != ctx.par->length) {
        ctx.par->length = ctx.offsetInPar;
        ctx.par->dirty = true;
    }

    try {
        const auto index = ctx.par - &m_vParagraphs.front();
        const auto itr = m_vParagraphs.begin() + (index + 1);
        m_vParagraphs.insert(itr, nextParagraph);
    }
    catch (...) {
        return CODE_OUTOFMEMORY;
    }

    m_cTextLength += m_sLineFeed.nlcount;
    m_bDirty = true;
    return CODE_OK;
}

bool BasicLayout::CISLayoutEngine::findPos(uint32_t pos, FindCtx& ctx) noexcept
{
    ctx = {};
    if (!m_cTextLength) {
        if (!this->baseParagraph())
            return false;
        ctx.par = &m_vParagraphs.front();
        ctx.cell = ctx.par->first;
        assert(ctx.cell == m_sHead.next);
        return true;
    }

    assert(m_vParagraphs.size() >= 2);
    if (m_vParagraphs.size() < 1)
        return false;

    Paragraph* paragraph = nullptr;
    Node* last = nullptr;
    auto current = pos;

    const auto begin = m_vParagraphs.begin();
    const auto end = m_vParagraphs.end() - 1;
    for (auto itr = begin; itr != end; ++itr) {
        auto& par = *itr;
        const uint32_t thisLine = static_cast<uint32_t>(par.length + m_sLineFeed.nlcount);
        if (current <= thisLine) {
            paragraph = &par;
            last = (itr + 1)->first;
            break;
        }
        current -= thisLine;
    }

    if (!paragraph || current > paragraph->length) {
        const auto lastPar = &m_vParagraphs.back() - 1;
        if (!paragraph || paragraph == lastPar) {
            const auto cell = static_cast<CISTextCell*>(m_sTail.prev);
            ctx.par = lastPar;
            ctx.cell = cell;
            ctx.offsetInPar = static_cast<uint32_t>(lastPar->length);
            ctx.offsetInCell = static_cast<uint32_t>(cell->GetView().len);
        }
        else {
            ctx.par = paragraph + 1;
            ctx.cell = ctx.par->first;
            assert(ctx.cell != &m_sTail);
            ctx.offsetInPar = 0;
            ctx.offsetInCell = 0;
        }
        return true;
    }

    const auto offsetInPar = current;

    Node* node = paragraph->first;
    while (node != last) {
        const auto cell = static_cast<CISTextCell*>(node);
        const auto thisCell = static_cast<uint32_t>(cell->GetView().len);
        if (current <= thisCell)
            break;
        current -= thisCell;
        node = node->next;
    }

    assert(node != last);
    if (node == last) {
        const auto cell = static_cast<CISTextCell*>(last->prev);
        node = cell;
        current = static_cast<uint32_t>(cell->GetView().len);
    }

    ctx.cell = static_cast<CISTextCell*>(node);
    ctx.par = paragraph;
    ctx.offsetInPar = offsetInPar;
    ctx.offsetInCell = current;

    return true;
}

void BasicLayout::CISLayoutEngine::paragraphRelayout(Paragraph& par) noexcept
{
    const auto limit = m_sSetting.canvasSize.width;
    const auto warp = limit > 0 && m_sSetting.wrapMode;
    const auto talign = m_sSetting.textAlign;
    const auto valign = m_sSetting.verticalAlign;
    const auto lineHeightDescent = m_sEnv.lineHeightDescent;
    const auto lineHeightAscent = m_sEnv.lineHeightAscent;

    const auto alignLayout = [valign, limit, talign, lineHeightDescent](Node* b, Node* e, const BaseCellBox& box, fp26dot6_t tail) noexcept {
        for (Node* node = b; node != e; node = node->next) {
            const auto cell = static_cast<CISTextCell*>(node);
            const auto cellbox = boxCast(cell->RefBox(), valign);
            cell->tail = tail;
            cell->position.y += box.ascent - cellbox.ascent;
        }
        if (talign && limit > 0) {
            const auto h2 = box.ascent + box.descent + lineHeightDescent;
            const auto lineFeedWidth = h2 / 2;
            fp26dot6_t move = limit - box.advance - lineFeedWidth;
            if (talign == TEXT_ALIGN_CENTER)
                move = move / 2;
            for (Node* node = b; node != e; node = node->next) {
                const auto cell = static_cast<CISTextCell*>(node);
                cell->position.x += move;
            }
        }
    };

    const auto begin = par.first;
    const auto end = (std::addressof(par) + 1)->first;
    BaseCellBox box = {};
    fp26dot6_t side = lineHeightAscent;
    bool newLine = false;
    Node* vlayout = begin;

    fp26dot6_t width = 0;

    for (Node* node = begin; node != end; node = node->next) {
        const auto cell = static_cast<CISTextCell*>(node);
        if (newLine) {
            newLine = false;
            side += box.ascent + box.descent + lineHeightDescent;
            alignLayout(vlayout, node, box, side);
            side += lineHeightAscent;
            vlayout = node;
            cell->MergeNext(end);
            width = std::max(box.advance, width);
            box = {};
        }
        auto cellbox = boxCast(cell->RefBox(), valign);
        if (warp) {
            const auto diff = box.advance + cellbox.advance - limit;
            if (diff > 0) {
                const auto pos = cell->SplitCharPos(cellbox.advance - diff, m_sSetting.wrapMode, !box.advance);
                cell->SplitWeak(pos);
                cellbox = boxCast(cell->RefBox(), valign);
                newLine = true;
            }
        }
        cell->position.x = box.advance;
        cell->head = side;
        cell->position.y = side;
        box.ascent = std::max(box.ascent, cellbox.ascent);
        box.descent = std::max(box.descent, cellbox.descent);
        box.advance += cellbox.advance;
    }
    const auto h2 = box.ascent + box.descent + lineHeightDescent;
    side += h2;
    alignLayout(vlayout, end, box, side);
    const auto lineFeedWidth = h2 / 2;
    width = std::max(box.advance, width) + lineFeedWidth;
    par.size = { width, side };
}

void BasicLayout::CISLayoutEngine::paragraphCheckLayout(Paragraph& par) noexcept
{
    if (par.dirty) {
        par.dirty = false;
        this->paragraphRelayout(par);
    }
}

void BasicLayout::CISLayoutEngine::checkLayout() noexcept
{
    if (m_bDirty) {
        m_bDirty = false;
        this->relayout();
    }
}

void BasicLayout::CISLayoutEngine::relayout() noexcept
{
    if (m_bNeedShrink) {
        m_bNeedShrink = false;
        this->doShrink();
    }
    m_fsDocSize = {};
    assert(!m_vParagraphs.empty());
    const auto beginPar = m_vParagraphs.begin();
    const auto endPar = m_vParagraphs.end() - 1;
    for (auto itr = beginPar; itr != endPar; ++itr) {
        auto& paragraph = *itr;
        this->paragraphCheckLayout(paragraph);
        m_fsDocSize.width = std::max(m_fsDocSize.width, paragraph.size.width);
        m_fsDocSize.height += paragraph.size.height;
    }
    this->relayoutTextAlign();
}

void BasicLayout::CISLayoutEngine::relayoutTextAlign() noexcept
{
    if (m_sSetting.textAlign && m_sSetting.canvasSize.width > 0) {
        fp26dot6_t move = m_fsDocSize.width - m_sSetting.canvasSize.width;
        const fp26dot6_t threshold = m_sSetting.canvasSize.width / 20;
        if (move > threshold) {
            if (m_sSetting.textAlign == TEXT_ALIGN_CENTER)
                move = move / 2;
            const auto begin = m_sHead.next;
            const auto end = &m_sTail;
            for (auto node = begin; node != end; node = node->next) {
                const auto cell = static_cast<CISTextCell*>(node);
                cell->position.x += move;
            }
        }
    }
}

static TextDraw& operator+=(TextDraw& left, TextDraw right) noexcept {
    left.count += right.count;
    left.drawn += right.drawn;
    return left;
}

TextDraw BasicLayout::CISLayoutEngine::Draw(CISTextRenderer* renderer, void* context, TextRenderParam param, const TextViewportRange* range) noexcept
{
    if (m_vParagraphs.empty())
        return {};

    this->checkLayout();
    fppoint_t point = {};
    TextDraw rc = {};

    const auto beginPar = m_vParagraphs.cbegin();
    const auto endPar = m_vParagraphs.cend() - 1;

    TextDocport viewport = {};
    TextDocport* viewptr = nullptr;
    if (range) {
        viewptr = &viewport;
        viewport.left = range->left;
        viewport.top = range->top;
        viewport.right = range->right;
        viewport.bottom = range->bottom;
    }

    uint32_t selectionBegin = m_sSelectionRange.position;
    uint32_t selectionEnd = selectionBegin + m_sSelectionRange.length;

    for (auto itr = beginPar; itr != endPar; ++itr) {
        const auto& paragraph = *itr;
        assert(paragraph.dirty == false);
        const auto begin = paragraph.first;
        const auto end = (std::addressof(paragraph) + 1)->first;

        point.x = 0;

        CISTextCell* last = nullptr;

        for (Node* node = begin; node != end; node = node->next) {
            const auto cell = static_cast<CISTextCell*>(node);
            last = cell;
            const auto len = static_cast<uint32_t>(cell->GetView().len);
            if (len) {
                rc += cell->Draw(renderer, point, context, param, selectionBegin, selectionEnd, viewptr);
                selectionBegin -= len;
                selectionEnd -= len;
            }
#ifndef NDEBUG
            else if (paragraph.length) {
                std::printf("[WARNING] %s: empty cell to draw\n", __FUNCTION__);
            }
#endif
        }

        if (last && selectionEnd && (selectionEnd < selectionBegin || !selectionBegin)) {
            last->DrawLineFeed(renderer, point, context, param, viewptr);
        }

        point.y += paragraph.size.height;
        selectionBegin -= m_sLineFeed.nlcount;
        selectionEnd -= m_sLineFeed.nlcount;
    }
    return rc;
}

void BasicLayout::CISLayoutEngine::SetTabWidth(fppoint_t, fppoint_t) noexcept
{
}

void BasicLayout::CISLayoutEngine::SetSelectionBlock(Range range) noexcept
{
    m_sSelectionRange = range;
}

uint32_t BasicLayout::CISLayoutEngine::FrontChar(uint32_t pos, bool word) noexcept
{
    if (!pos)
        return 0;
    FindCtx ctx;
    if (!this->findPos(pos, ctx))
        return 0;

    char32_t first = 0;

    const auto paragraph = ctx.par;
    auto posTmp = pos;
    Node* const end = paragraph->first->prev;
    Node* const begin = ctx.cell;
    uint32_t remain = ctx.offsetInCell;
    for (Node* node = begin; node != end; node = node->prev) {
        const auto cell = static_cast<CISTextCell*>(node);
        const auto view = cell->GetView();
        posTmp -= remain;
        if (remain) {
            const auto ret = cell->FindFrontChar(first, remain, word);
            if (ret <= static_cast<uint32_t>(view.len))
                return posTmp + ret;
        }
        remain = static_cast<uint32_t>(view.len);
    }

    const auto num = paragraph - m_vParagraphs.data();
    if (num) {
        const uint32_t move = ctx.offsetInPar + m_sLineFeed.nlcount;
        if (pos >= move)
            return pos - move;
        assert(!"ABORT");
    }
    return 0;
}

uint32_t BasicLayout::CISLayoutEngine::BackChar(uint32_t pos, bool word) noexcept
{
    FindCtx ctx;
    if (!this->findPos(pos, ctx))
        return 0;
    const auto paragraph = ctx.par;

    char32_t first = 0;
    Node* const begin = ctx.cell;
    Node* const end = paragraph[1].first;
    uint32_t offset = ctx.offsetInCell;
    uint32_t posTmp = pos - offset;
    for (Node* node = begin; node != end; node = node->next) {
        const auto cell = static_cast<CISTextCell*>(node);
        const auto len = static_cast<uint32_t>(cell->GetView().len);
        const auto ret = cell->FindBackChar(first, offset, word);
        if (ret <= len)
            return posTmp + ret;
        posTmp += len;
        offset = 0;
    }

    const auto num = &m_vParagraphs.back() - paragraph;
    if (num > 1) {
        if (pos >= ctx.offsetInPar) {
            return pos - ctx.offsetInPar + static_cast<uint32_t>(paragraph->length) + m_sLineFeed.nlcount;
        }
        assert(!"ABORT");
    }

    return m_cTextLength;
}

uint32_t BasicLayout::CISLayoutEngine::RaiseChar(uint32_t index, fp26dot6_t pos) noexcept
{
    this->checkLayout();
    HitTestCtx ctx;
    this->HitTest(index, ctx);

    auto prev = ctx.caret1;
    prev.y -= 64 / 2;
    if (pos >= 0)
        prev.x = pos;
    this->HitTest(prev, ctx);
    return ctx.postion;
}

uint32_t BasicLayout::CISLayoutEngine::LowerChar(uint32_t index, fp26dot6_t pos) noexcept
{
    this->checkLayout();
    HitTestCtx ctx;
    this->HitTest(index, ctx);

    auto next = ctx.caret2;
    next.y += 64 / 2;
    if (pos >= 0)
        next.x = pos;
    this->HitTest(next, ctx);
    return ctx.postion;
}

uint32_t BasicLayout::CISLayoutEngine::LineBegin(uint32_t pos) noexcept
{
    FindCtx ctx;
    if (!this->findPos(pos, ctx))
        return 0;

    return pos - ctx.offsetInPar;
}

uint32_t BasicLayout::CISLayoutEngine::LineEnd(uint32_t pos) noexcept
{
    FindCtx ctx;
    if (!this->findPos(pos, ctx))
        return m_cTextLength;

    return pos + ctx.par->length - ctx.offsetInPar;
}

void BasicLayout::CISLayoutEngine::dirtyAllParagraphs() noexcept
{
    m_bDirty = true;
    for (auto& p : m_vParagraphs) p.dirty = true;
}

template<typename T>
void BasicLayout::CISLayoutEngine::dirtyAll(T func) noexcept
{
    m_bDirty = true;
    auto node = m_sHead.next;
    while (node != &m_sTail) {
        const auto cell = static_cast<CISTextCell*>(node);
        node = node->next;
        func(cell);
    }
    for (auto& p : m_vParagraphs) p.dirty = true;
}

void BasicLayout::CISLayoutEngine::SetKerning(bool on) noexcept
{
    if (m_sEnv.kerning == on)
        return;
    m_sEnv.kerning = on;
    this->dirtyAll([](CISTextCell* cell) noexcept {
        cell->SetKerning();
    });
}

void BasicLayout::CISLayoutEngine::SetPasswordMode(char32_t ch) noexcept
{
    if (m_chPassword == ch)
        return;
    m_chPassword = ch;
    this->dirtyAll([ch](CISTextCell* cell) noexcept {
        cell->SetPasswordMode(ch);
    });
}

void BasicLayout::CISLayoutEngine::SetSingleLine(bool on) noexcept
{
    m_bSingleLine = on;
}

void BasicLayout::CISLayoutEngine::SetCanvasSize(fpsize_t canvas) noexcept
{
    m_sSetting.canvasSize = canvas;
    if (m_sSetting.wrapMode || m_sSetting.textAlign) {
        this->dirtyAllParagraphs();
    }
}

CODE BasicLayout::CISLayoutEngine::SetWrapMode(WRAP_MODE mode) noexcept
{
    if (m_sSetting.wrapMode == mode)
        return CODE_FALSE;
    m_sSetting.wrapMode = mode;
    this->dirtyAllParagraphs();
    return CODE_OK;
}

CODE BasicLayout::CISLayoutEngine::SetTextAlign(TEXT_ALIGN align) noexcept
{
    if (m_sSetting.textAlign == align)
        return CODE_FALSE;
    if (align >= TEXT_ALIGN_JUSTIFIED)
        return CODE_NOTIMPL;
    m_sSetting.textAlign = align;
    this->dirtyAllParagraphs();
    return CODE_OK;
}

CODE BasicLayout::CISLayoutEngine::SetVerticalAlign(VERTICAL_ALIGN align) noexcept
{
    if (m_sSetting.verticalAlign == align)
        return CODE_FALSE;
    m_sSetting.verticalAlign = align;
    this->dirtyAllParagraphs();
    return CODE_OK;
}
