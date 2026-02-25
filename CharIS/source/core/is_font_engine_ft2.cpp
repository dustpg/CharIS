#include "is_font_engine_ft2.h"
#include "is_ft2_helper.h"
#include "../core/is_helper_c.h"
#include <cassert>
#include <string_view>
#include <algorithm>
#include FT_OUTLINE_H
#include FT_MODULE_H
#include FT_TRUETYPE_TABLES_H  
#include FT_TYPE1_TABLES_H
#include FT_LCD_FILTER_H  

namespace CharIS { namespace impl {

    static inline void unit_map(int16_t& value, int32_t unit) {
        value = int32_t(value) * FONT_METRICS_UNIT / unit;
    }
    static inline void unit_map(int32_t& value, int32_t unit) {
        value = value * FONT_METRICS_UNIT / unit;
    }
}}

using namespace CharIS;

size_t FreeType::FONT_HASH::impl(const FontEx& ts) noexcept
{
    std::u16string_view view{ ts.font.family, ts.font.length };
    const size_t code = std::hash<std::u16string_view>()(view);
    const size_t base = reinterpret_cast<const uint32_t&>(ts.font.weight);
    return code ^ base;
}

bool FreeType::FONT_EQUL::operator()(const FontEx& l, const FontEx& r) const noexcept
{
    if (l.font.length == r.font.length) {
        return !std::memcmp(&l.font, &r.font, 6 + l.font.length * sizeof(char16_t));
    }
    return false;
}


namespace CharIS { namespace impl {

    struct tf_ctx {
        IISGeometrySink* sink;
        double scale;
    };

    /// <summary>
    /// Tses the cast.
    /// </summary>
    /// <param name="p">The p.</param>
    /// <returns></returns>
    static inline auto charis_cast(const FT_Vector* p, double scale) noexcept { 
        return PointF{ double(p->x) * scale, double(p->y) * scale }; 
    };

    /// <summary>
    /// Tses the tf move to function.
    /// </summary>
    /// <param name="to">To.</param>
    /// <param name="user">The user.</param>
    /// <returns></returns>
    static int tf_move_to_func(const FT_Vector* to, void* user) noexcept {
        const auto ctx = *reinterpret_cast<tf_ctx*>(user);
        //const auto code =
            ctx.sink->MoveTo(charis_cast(to, ctx.scale));
        return 0;
    };

    /// <summary>
    /// Tses the tf line to function.
    /// </summary>
    /// <param name="to">To.</param>
    /// <param name="user">The user.</param>
    /// <returns></returns>
    static int tf_line_to_func(const FT_Vector* to, void* user) noexcept {
        const auto ctx = *reinterpret_cast<tf_ctx*>(user);
        //const auto code = 
            ctx.sink->LineTo(charis_cast(to, ctx.scale));
        return 0;
    };

    /// <summary>
    /// Tses the tf conic to function.
    /// </summary>
    /// <param name="control">The control.</param>
    /// <param name="to">To.</param>
    /// <param name="user">The user.</param>
    /// <returns></returns>
    static int tf_conic_to_func(const FT_Vector* control, const FT_Vector* to, void* user) noexcept {
        const auto ctx = *reinterpret_cast<tf_ctx*>(user);
        //const auto code = 
            ctx.sink->ConicTo(charis_cast(control, ctx.scale), charis_cast(to, ctx.scale));
        return 0;
    };

    /// <summary>
    /// Tses the tf cubic to function.
    /// </summary>
    /// <param name="control1">The control1.</param>
    /// <param name="control2">The control2.</param>
    /// <param name="to">To.</param>
    /// <param name="user">The user.</param>
    /// <returns></returns>
    static int tf_cubic_to_func(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) noexcept {
        const auto ctx = *reinterpret_cast<tf_ctx*>(user);
        //const auto code = 
            ctx.sink->CubicTo(
            charis_cast(control1, ctx.scale), charis_cast(control2, ctx.scale), charis_cast(to, ctx.scale)
        );
        return 0;
    };


    /// <summary>
    /// Tses the font ft io.
    /// </summary>
    /// <param name="stream">The stream.</param>
    /// <param name="offset">The offset.</param>
    /// <param name="buffer">The buffer.</param>
    /// <param name="count">The count.</param>
    /// <returns></returns>
    static unsigned long font_ft_io(FT_Stream stream,
        unsigned long   offset,
        unsigned char* buffer,
        unsigned long   count) noexcept {
        return CharIS::FtFileRead(stream->descriptor.pointer, offset, buffer, count);
    }

    /// <summary>
    /// Tses the font ft close.
    /// </summary>
    /// <param name="stream">The stream.</param>
    /// <returns></returns>
    static void font_ft_close(FT_Stream stream) noexcept {
        CharIS::FtFileClose(CharIS::Take(stream->descriptor.pointer));
    }


}}



/// <summary>
/// Frees the type e code.
/// </summary>
/// <param name="code">The code.</param>
/// <returns></returns>
CODE CharIS::FtError(int code) noexcept {
    switch (code)
    {
    case 0:
        return CODE_OK;
    case FT_Err_Invalid_Argument:
        return CODE_INVALIDARG;
    case FT_Err_Out_Of_Memory:
        return CODE_OUTOFMEMORY;
    default:
        return CODE_FAILED;
    }
}

/// <summary>
/// Createds the specified .
/// </summary>
inline void FreeType::CISFontEngine::Created(const CISFontFace& font) noexcept
{
    m_uCounter++;
}

inline void FreeType::CISFontEngine::Disposed(const CISFontFace& font) noexcept
{
    m_uCounter--;
}


/// <summary>
/// Advs the index.
/// </summary>
/// <returns></returns>

// ----------------------------------------------------------------------------
//                               CISFaceGlyph
// ----------------------------------------------------------------------------

uintptr_t FreeType::CISFaceGlyph::GetHandle() const noexcept
{
    return uintptr_t(m_hFontFace);
}


void FreeType::CISFaceGlyph::GetGlyphMetrics(GlyphMetrics* metrics) noexcept
{
    assert(metrics && m_hFontFace);
    auto& m = m_hFontFace->glyph->metrics;

    metrics->leftSideBearing = m.horiBearingX;
    metrics->advanceWidth = m.horiAdvance;
    metrics->rightSideBearing = m.horiAdvance - m.horiBearingX - m.width;

    metrics->topSideBearing = m.vertBearingY;
    metrics->advanceHeight = m.vertAdvance;
    metrics->bottomSideBearing = m.vertAdvance - m.vertBearingY - m.height;

    metrics->verticalOriginY = m.horiBearingY + metrics->topSideBearing;

    metrics->points = m_hFontFace->glyph->outline.n_points;
    metrics->contours = m_hFontFace->glyph->outline.n_contours;

#if 1
    const auto units = m_hFontFace->units_per_EM;
    impl::unit_map(metrics->leftSideBearing, units);
    impl::unit_map(metrics->advanceWidth, units);
    impl::unit_map(metrics->rightSideBearing, units);
    impl::unit_map(metrics->topSideBearing, units);
    impl::unit_map(metrics->advanceHeight, units);
    impl::unit_map(metrics->bottomSideBearing, units);
    impl::unit_map(metrics->verticalOriginY, units);
#endif
}

CODE CharIS::FreeType::CISFaceGlyph::Init(const char16_t* path, uint32_t index, CISFontEngine* engine) noexcept
{
    assert(m_hFontFace == nullptr);
    CODE code = CODE_OK;
    assert(engine && path);
    const auto ft = engine->Freetype();
    assert(ft);
    
    // LOAD FILE
    if (Success(code)) {
        m_sStream.descriptor.pointer = CharIS::FtOpenFile(path);
        m_sStream.size = CharIS::FtFileSize(m_sStream.descriptor.pointer);
        m_sStream.read = impl::font_ft_io;
        m_sStream.close = impl::font_ft_close;

        FT_Open_Args open_args = {};
        open_args.flags = FT_OPEN_STREAM;
        open_args.stream = &m_sStream;
        FT_Error error = ::FT_Open_Face(ft, &open_args, index, &m_hFontFace);
        if (error) {
            CHARIS_LOGGER_ERR("FT_Open_Face failed: %d\n", error);
            code = CharIS::FtError(error);
        }
    }
    
    return code;
}


/// <summary>
/// Gets the index.
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
uint32_t FreeType::CISFaceGlyph::GetIndex(char32_t ch) noexcept
{
    assert(m_hFontFace);
    const auto glyph_index = ::FT_Get_Char_Index(m_hFontFace, ch);
    return glyph_index;
}

/// <summary>
/// Loads the glyph.
/// </summary>
/// <param name="index">The index.</param>
/// <returns></returns>
CODE FreeType::CISFaceGlyph::LoadGlyph(uint32_t index, int flag) noexcept
{
    auto error = ::FT_Load_Glyph(m_hFontFace, index, flag);

    if (error) {
        CHARIS_LOGGER_ERR("FT_Load_Glyph failed: %d\n", error);
        assert(!("FT_Load_Glyph failed"));
    }

    return CharIS::FtError(error);
}

//void CharIS::FreeType::CISFaceGlyph::Move(CISFaceGlyph&& other) noexcept
//{
//    assert(this != &other);
//    std::swap(other.m_hFontFace, m_hFontFace);
//    std::swap(other.m_sStream, m_sStream);
//    if (m_hFontFace) {
//        m_hFontFace->stream = &m_sStream;
//    }
//    if (other.m_hFontFace) {
//        other.m_hFontFace->stream = &other.m_sStream;
//    }
//}

CharIS::FreeType::CISFaceGlyph::CISFaceGlyph() noexcept
{
}

//void CharIS::FreeType::CISFaceGlyph::Done() noexcept
//{
//    if (m_hFontFace) {
//        ::FT_Done_Face(CharIS::Take(m_hFontFace));
//    }
//}


CharIS::FreeType::CISFaceGlyph::~CISFaceGlyph() noexcept
{
    if (m_hFontFace) {
        ::FT_Done_Face(CharIS::Take(m_hFontFace));
    }
}

int32_t FreeType::CISFaceGlyph::GetKerning(char32_t left, char32_t right) noexcept
{
    if (!m_hFontFace)
        return 0;
    const auto l = ::FT_Get_Char_Index(m_hFontFace, left);
    const auto r = ::FT_Get_Char_Index(m_hFontFace, right);
    FT_Vector adjust;
    const auto error = ::FT_Get_Kerning(m_hFontFace, l, r, FT_KERNING_UNSCALED, &adjust);
    if (error)
        return 0;
    return adjust.x;
}

CODE FreeType::CISFaceGlyph::GetOutline(IISGeometrySink* sink, double size) noexcept
{
    assert(sink && m_hFontFace);
    FT_GlyphSlot slot = m_hFontFace->glyph;
    FT_Outline& outline = slot->outline;
    FT_Outline_Funcs callback = {};

    const double unit = m_hFontFace->units_per_EM;

    impl::tf_ctx ctx{ sink, size / unit };

    callback.move_to = impl::tf_move_to_func;
    callback.line_to = impl::tf_line_to_func;
    callback.conic_to = impl::tf_conic_to_func;
    callback.cubic_to = impl::tf_cubic_to_func;

    const auto error = ::FT_Outline_Decompose(&outline, &callback, &ctx);
    if (error) {
        CHARIS_LOGGER_ERR("FT_Outline_Decompose failed: %d\n", error);
    }
    return CharIS::FtError(error);
}

CODE FreeType::CISFaceGlyph::RenderToBitmap(fp26dot6_t size, BitmapInfo* info, BITMA_PRENDER_MODE mode) noexcept
{
    assert(m_hFontFace);
    if (!m_hFontFace)
        return CODE_UNEXPECTED;
    FT_Render_Mode ftmode = FT_RENDER_MODE_NORMAL;
    uint32_t pixelWidth = 1;
    uint32_t pixelHeight = 1;
    BITMAP_PIXEL_FORAMT fmt = BITMAP_PIXEL_FORAMT_R8;
    switch (mode)
    {
    case BITMA_PRENDER_MODE_NORMAL:
        ftmode = FT_RENDER_MODE_NORMAL;
        pixelWidth = 1;
        break;
    case BITMA_PRENDER_MODE_MONO:
        ftmode = FT_RENDER_MODE_MONO;
        pixelWidth = 1;
        break;
    case BITMA_PRENDER_MODE_LCD_HORIZONTAL: 
        ftmode = FT_RENDER_MODE_LCD;
        fmt = BITMAP_PIXEL_FORAMT_RGB8;
        pixelWidth = 3;
        break;
    case BITMA_PRENDER_MODE_LCD_VERTICAL: 
        ftmode = FT_RENDER_MODE_LCD_V;
        pixelHeight = 3;
        fmt = BITMAP_PIXEL_FORAMT_RGB8_PLANARLINE;
        break;
    }


    FT_Error code = ::FT_Set_Char_Size(m_hFontFace, size, size, 0, 0);
    if (!code) {
        const auto index = m_hFontFace->glyph->glyph_index;
        code = ::FT_Load_Glyph(m_hFontFace, index, FT_LOAD_DEFAULT);
        //code = ::FT_Load_Glyph(m_hFontFace, index, FT_LOAD_NO_HINTING);
    }
    if (!code) {
        code = ::FT_Render_Glyph(m_hFontFace->glyph, ftmode);
    }

    if (!code) {
        const auto slot = m_hFontFace->glyph;
        const auto& bmp = slot->bitmap;
        assert(bmp.num_grays == 256);
        info->data = bmp.buffer;
        info->width = bmp.width / pixelWidth;
        info->height = bmp.rows / pixelHeight;
        info->pitch = bmp.pitch;
        info->format = fmt;
        info->offsetX = slot->bitmap_left;
        info->offsetY = slot->bitmap_top;
    }

    if (code) {
        CHARIS_LOGGER_ERR("FT_Render_Glyph failed: %d\n", code);
    }

    return CharIS::FtError(code);
}

// ----------------------------------------------------------------------------
//                               CISFaceCore
// ----------------------------------------------------------------------------

//void CharIS::FreeType::CISFaceCore::Done() noexcept
//{
//    for (auto& item : m_map)
//        item.Done();
//}

FreeType::CISFaceCore::~CISFaceCore() noexcept
{
    assert(m_bitmap == (1 << FT_MAX_THREAD_INSTANCE) - 1);
}

FreeType::CISFaceCore::CISFaceCore(const Font& ts) noexcept : m_sFontData(ts)
{
}

CODE FreeType::CISFaceCore::Init(CISFontEngine* engine) noexcept
{
    CODE code = CODE_OK;
    assert(engine);
    const auto ft = engine->Freetype();
    const auto db = engine->Database();
    assert(ft && db);
    constexpr uint32_t pathlen = 1024;
    char16_t path[pathlen];
    uint32_t index = 0;
    
    // FIND OUT
    if (Success(code)) {
        code = db->FindFont(
            m_sFontData.family,
            FONT_WEIGHT(m_sFontData.weight),
            FONT_STRETCH(m_sFontData.stretch),
            FONT_STYLE(m_sFontData.style),
            pathlen,
            path,
            index
        );
    }
    
    // LAZY INIT: Only initialize the first glyph instance (m_map[0])
    if (Success(code)) {
        code = m_map[0].Init(path, index, engine);
    }
    
    // INIT FONT METRICS from the first face
    if (Success(code)) {
        const auto face = m_map[0].GetFace();
        assert(face);
        m_sFontMetrics.ascent = face->ascender;
        m_sFontMetrics.descent = -face->descender;

        m_sFontMetrics.underlineThickness = face->underline_thickness;
        m_sFontMetrics.underlinePosition = face->underline_position;

        // TT_OS2 table, fields yStrikeoutSize and yStrikeoutPosition
        if (const auto os2 = (TT_OS2*)::FT_Get_Sfnt_Table(face, FT_SFNT_OS2)) {
            m_sFontMetrics.strikethroughPosition = os2->yStrikeoutPosition;
            m_sFontMetrics.strikethroughThickness = os2->yStrikeoutSize;
            m_sFontMetrics.weiget = os2->usWeightClass;
        }
        else {
            m_sFontMetrics.strikethroughPosition = face->units_per_EM / 3;
            m_sFontMetrics.strikethroughThickness = face->underline_thickness;
            m_sFontMetrics.weiget = m_sFontData.weight;
        }
        const auto units = face->units_per_EM;
        impl::unit_map(m_sFontMetrics.ascent, units);
        impl::unit_map(m_sFontMetrics.descent, units);
        impl::unit_map(m_sFontMetrics.underlinePosition, units);
        impl::unit_map(m_sFontMetrics.underlineThickness, units);
        impl::unit_map(m_sFontMetrics.strikethroughPosition, units);
        impl::unit_map(m_sFontMetrics.strikethroughThickness, units);
        m_sFontMetrics.kerningSupport = !!(face->face_flags & FT_FACE_FLAG_KERNING);
    }
    
    return code;
}

CODE FreeType::CISFaceCore::Borrow(CISFontEngine* engine, CISFaceGlyph** output) noexcept
{
    assert(engine && output);
    const auto bitmap = m_bitmap;
    if (bitmap == 0)
        return CODE_SMALLBUFFER;
    const uint32_t i = charis_trailing_zeros_u32(bitmap);
    // strong code: should not happen
    if (i >= FT_MAX_THREAD_INSTANCE){
        assert(!("i >= FT_MAX_THREAD_INSTANCE"));
        return CODE_SMALLBUFFER;
    }
    m_bitmap &= ~(1u << i);
    CISFaceGlyph* glyph = &m_map[i];
    CODE code = CODE_OK;
    if (!glyph->GetFace()) {
        const auto ft = engine->Freetype();
        const auto db = engine->Database();
        assert(ft && db);
        constexpr uint32_t pathlen = 1024;
        char16_t path[pathlen];
        uint32_t index = 0;
        // FIND OUT
        code = db->FindFont(
            m_sFontData.family,
            FONT_WEIGHT(m_sFontData.weight),
            FONT_STRETCH(m_sFontData.stretch),
            FONT_STYLE(m_sFontData.style),
            pathlen,
            path,
            index
        );
        if (Success(code)) {
            code = db->FindFont(
                m_sFontData.family,
                FONT_WEIGHT(m_sFontData.weight),
                FONT_STRETCH(m_sFontData.stretch),
                FONT_STYLE(m_sFontData.style),
                pathlen,
                path,
                index
            );
        }
        if (Success(code)) {
            code = glyph->Init(path, index, engine);
        }
    }
    if (Success(code)) {
        *output = glyph;
        return code;
    }
    else {
        m_bitmap |= (1u << i);
        if (code == CODE_SMALLBUFFER)
            code = CODE_FAILED;
        return code;
    }
}

void FreeType::CISFaceCore::Return(uint32_t index) noexcept
{
    assert(index < FT_MAX_THREAD_INSTANCE);
    m_bitmap |= (1u << index);
}

uint32_t FreeType::CISFaceCore::Returnable(CISFaceGlyph* glyph) const noexcept
{
    if (!glyph)
        return UINT32_MAX;
    const auto diff = glyph - m_map;
    return static_cast<uint32_t>(static_cast<size_t>(diff));
}

//void CharIS::FreeType::CISFaceCore::Move(CISFaceCore&& other) noexcept
//{
//    std::swap(other.m_sFontData, m_sFontData);
//    std::swap(other.m_sFontMetrics, m_sFontMetrics);
//    assert(other.m_bitmap == m_bitmap);
//    for (auto i = 0; i < FT_MAX_THREAD_INSTANCE; i++) {
//        m_map[i].Move(std::move(other.m_map[i]));
//    }
//}

// ----------------------------------------------------------------------------
//                                 CISFontFace
// ----------------------------------------------------------------------------

FreeType::CISFontFace::~CISFontFace() noexcept
{
    m_refEngine.Disposed(*this);
    CHARIS_LOGGER_LOG("FontFaceRemoved: %p\n", this);
    impl::mtx_destroy(&m_mtx);
    impl::cnd_destroy(&m_cnd);
}

//CharIS::FreeType::CISFontFace::CISFontFace(CISFontEngine& engine) noexcept
//    : m_refEngine(engine)
//{
//    impl::mtx_init(&m_mtx);
//    impl::cnd_init(&m_cnd);
//    CHARIS_LOGGER_LOG("FontFaceCreated(FONT): %p\n", this);
//}

FreeType::CISFontFace::CISFontFace(CISFontEngine& engine, const Font& ts) noexcept 
    : m_refEngine(engine)
    , m_core(ts)
{
    impl::mtx_init(&m_mtx);
    impl::cnd_init(&m_cnd);
    CHARIS_LOGGER_LOG("FontFaceCreated(FONT): %p\n", this);
}

//FreeType::CISFontFace::CISFontFace(CISFontFace&& obj) noexcept
//    : m_wpEngine(obj.m_wpEngine)
//    , m_core(std::move(obj.m_core))
//    , m_uCurrentGlyph(obj.m_uCurrentGlyph)
//{
//    if (auto mgr = m_wpEngine.ptr) {
//        mgr->Created(*this);
//    }
//    CHARIS_LOGGER_LOG("FontFaceCreated(MOVE): %p\n", this);
//}

CODE FreeType::CISFontFace::Init() noexcept
{
    CODE code = CODE_OK;
    const auto engine = &m_refEngine;
    engine->Created(*this);
    return m_core.Init(engine);
}

const Font* FreeType::CISFontFace::GetFontData() const noexcept
{
    return m_core.GetFontData();
}

const FontMetrics* FreeType::CISFontFace::GetFontMetrics() const noexcept
{
    return m_core.GetFontMetrics();
}

CODE CharIS::FreeType::CISFontFace::LockBase(CISFaceGlyph** outGlyph) noexcept
{
    assert(outGlyph);
    const auto engine = &m_refEngine;
    impl::mtx_lock(&m_mtx);
    CODE code;

    while (true) {
        code = m_core.Borrow(engine, outGlyph);
        if (code != CODE_SMALLBUFFER)
            break;
        // Wait for a glyph to become available
        impl::cnd_wait(&m_cnd, &m_mtx);
    }

    impl::mtx_unlock(&m_mtx);

    return code;
}

CODE FreeType::CISFontFace::Lock(char32_t ch, IISFontFaceGlyph** outGlyph) noexcept
{
    if (!outGlyph)
        return CODE_POINTER;

    CISFaceGlyph* glyph = nullptr;
    CODE code = this->LockBase(&glyph);
    if (Success(code)) {
        auto index = glyph->GetIndex(ch);
        if (!index) {
            CISFaceGlyph* fallback = nullptr;
            const auto fb = m_refEngine.FallbackLock(ch, &fallback);
            if (fb.index) {
                index = fb.index;
                std::swap(glyph, fallback);
                this->Unlock(fallback);
            }
        }

        code = glyph->LoadGlyph(index, FT_LOAD_NO_SCALE);
    }

    if (Success(code)) {
        *outGlyph = glyph;
        return code;
    }
    else {
        *outGlyph = nullptr;
        if (glyph) {
            this->Unlock(glyph);
        }
        return code;
    }
}

bool CharIS::FreeType::CISFontFace::Move() noexcept
{
    return m_refEngine.TryMove(*this);
}

void CharIS::FreeType::CISFontFace::UnlockBase(uint32_t index) noexcept
{
    assert(index < FT_MAX_THREAD_INSTANCE);
    impl::mtx_lock(&m_mtx);
    m_core.Return(index);
    impl::cnd_signal(&m_cnd);
    impl::mtx_unlock(&m_mtx);
}

void FreeType::CISFontFace::Unlock(IISFontFaceGlyph* glyph) noexcept
{
    if (glyph) {
        const auto index = m_core.Returnable(static_cast<CISFaceGlyph*>(glyph));
        // RIGHT HERE
        if (index < FT_MAX_THREAD_INSTANCE) {
            this->UnlockBase(index);
        }
        // FALL BACK
        else {
            m_refEngine.FallbackUnlock(static_cast<CISFaceGlyph*>(glyph));
        }
    }
}


// ----------------------------------------------------------------------------
//                                 CISFontEngine
// ----------------------------------------------------------------------------


/// <summary>
/// Advs the index.
/// </summary>
/// <returns></returns>
inline void FreeType::CISFontEngine::advIndex() noexcept
{
    m_uRandomIndex++;
    m_uRandomIndex = m_uRandomIndex % uint32_t(FT_CACHE_QUEUE_LEN);
}

/// <summary>
/// Finalizes an instance of the <see cref="CISFontEngine"/> class.
/// </summary>
/// <returns></returns>
FreeType::CISFontEngine::~CISFontEngine() noexcept
{
    this->discard();
    //impl::mtx_destroy(&m_mtxFallback);
}

/// <summary>
/// Clears the global fallback list.
/// </summary>
/// <returns></returns>
void FreeType::CISFontEngine::clear_global_fallback() noexcept
{
    for (auto& obj : m_vGlobalFallbackList)
        CharIS::SafeDispose(obj);

    m_vGlobalFallbackList.clear();
}

void FreeType::CISFontEngine::clear_cache() noexcept
{
    for (auto& item : m_apCached) {
        CharIS::SafeDispose(item);
    }
}

/// <summary>
/// Discards this instance.
/// </summary>
/// <returns></returns>
void FreeType::CISFontEngine::discard() noexcept
{
    this->clear_global_fallback();
    this->clear_cache();

    assert(m_uCounter == 0);
    if (m_uCounter) {
        CHARIS_LOGGER_ERR("font face obj exit: %d\n", int(m_uCounter));
    }
    ::FT_Done_FreeType(CharIS::Take(m_hLibrary));
    CharIS::SafeDispose(m_pFontProvider);
}



/// <summary>
/// Initializes a new instance of the <see cref="CISFontEngine"/> class.
/// </summary>
FreeType::CISFontEngine::CISFontEngine() noexcept
{
    //impl::mtx_init(&m_mtxFallback);
}


/// <summary>
/// Initializes this instance.
/// </summary>
/// <param name="database">The database.</param>
/// <returns></returns>
CODE FreeType::CISFontEngine::Init(IISFontProvider* database) noexcept
{
    assert(m_uCounter == 0);
    //this->discard();
    m_pFontProvider = database;
    if (!m_pFontProvider)
        return CODE_POINTER;
    m_pFontProvider->AddRefCnt();
    CODE code = CODE_OK;
    // FT2
    if (Success(code)) {
        int error = ::FT_Init_FreeType(&m_hLibrary);
        if (error) {
            CHARIS_LOGGER_ERR("FT_Init_FreeType failed: %d\n", error);
            code = CharIS::FtError(error);
        }
        else {
            //// Enable stem darkening for better readability at small sizes
            //FT_Library_SetLcdFilter(m_hLibrary, FT_LCD_FILTER_DEFAULT);
            //FT_Bool no_stem_darkening = 0;
            //FT_Property_Set(m_hLibrary, "cff", "no-stem-darkening", &no_stem_darkening);
            //FT_Property_Set(m_hLibrary, "autofitter", "no-stem-darkening", &no_stem_darkening);
        }
    }
    return code;
}


/// <summary>
/// Sets the global fallback list.
/// </summary>
/// <param name="list">The list.</param>
/// <returns></returns>
CODE FreeType::CISFontEngine::SetGlobalFallbackList(const char16_t* list) noexcept
{
    this->clear_global_fallback();

    try {
        // COUNT THE ','
        uint32_t count = 0;
        auto itr = list;
        while (*itr) {
            if (*itr == ',') ++count;
            ++itr;
        }
        m_vGlobalFallbackList.resize(count + 1);
    }
    catch (...) { return  CODE_OUTOFMEMORY; }


    const auto trimAndMatch = [this](const char16_t* front, const char16_t* back, CISFontFace** data) noexcept {
        while (front < back && *front == ' ') {
            front++;
        }
        while (front < back && back[-1] == ' ') {
            back--;
        }
        return this->matchFontFaceEx(
            reinterpret_cast<IISFontFace**>(data),
            front, uint32_t(back - front),
            FONT_WEIGHT_NORMAL, FONT_STYLE_NORMAL, FONT_STRETCH_NORMAL
        );
    };

    auto ptr = m_vGlobalFallbackList.data();

    CODE error = CODE_OK;

    auto front = list;
    auto back = list;

    while (*back) {
        if (*back == ',') {
            const auto code = trimAndMatch(front, back, ptr);
            if (Failure(code))
                error = code;
            ++ptr;
            front = back + 1;
        }
        ++back;
    };

    const auto code = trimAndMatch(front, back, ptr);
    if (Failure(code))
        error = code;

    auto& vec = m_vGlobalFallbackList;

    const auto len1 = vec.size();
    vec.erase(std::remove(vec.begin(), vec.end(), nullptr), vec.end());
    const auto len2 = vec.size();

    if (len2)
        return len2 == len1 ? CODE_OK : CODE_FALSE;
    else
        return error;
}


/// <summary>
/// Sets the specific fallback list.
/// </summary>
/// <param name="name">The name.</param>
/// <param name="list">The list.</param>
/// <returns></returns>
//CODE FreeType::CISFontEngine::SetSpecificFallbackList(const char16_t* name, const char16_t* list) noexcept
//{
//    assert(!"NOTIMPL");
//    return CODE_NOTIMPL;
//}

auto CharIS::FreeType::CISFontEngine::FallbackLock(char32_t ch, CISFaceGlyph** output) noexcept -> IndexResult
{
    assert(output);
    for (auto face : m_vGlobalFallbackList) {
        CISFaceGlyph* tmp = nullptr;
        const auto code = face->LockBase(&tmp);
        if (Success(code)) {
            if (const auto index = tmp->GetIndex(ch)) {
                *output = tmp;
                return { CODE_OK, index };
            }
            face->Unlock(tmp);
        }
    }
    return { CODE_FILE_NOT_FOUND, 0 };
}

void CharIS::FreeType::CISFontEngine::FallbackUnlock(CISFaceGlyph* glyph) noexcept
{
    assert(glyph);

    for (auto face : m_vGlobalFallbackList) {
        const auto index = face->RefCore().Returnable(glyph);
        if (index < FT_MAX_THREAD_INSTANCE) {
            face->UnlockBase(index);
            return;
        }
    }

    CHARIS_LOGGER_ERR("FallbackUnlock: %p not found\n", glyph);
}

/// <summary>
/// Fallbacks the specified ch.
/// </summary>
CODE FreeType::CISFontEngine::MatchFontFace(IISFontFace** face, const char16_t* name, FONT_WEIGHT w, FONT_STYLE se, FONT_STRETCH sh) noexcept
{
    char16_t defaultName[80];
    // DEFAULT FONT
    if (!name || !*name) {
        name = defaultName;
        const auto code = m_pFontProvider->DefaultFont(defaultName, sizeof(defaultName) / sizeof(defaultName[0]));
        if (Failure(code))
            return code;
    }
    return matchFontFaceEx(face, name, uint32_t(std::char_traits<char16_t>::length(name)), w, se, sh);
}

/// <summary>
/// Matches the font face ex2.
/// </summary>
/// <param name="face">The face.</param>
/// <param name="name">The name.</param>
/// <param name="len">The length.</param>
/// <param name="w">The w.</param>
/// <param name="se">The se.</param>
/// <param name="sh">The sh.</param>
/// <returns></returns>
CODE FreeType::CISFontEngine::matchFontFaceEx(IISFontFace** face, const char16_t* name, uint32_t len, FONT_WEIGHT w, FONT_STYLE se, FONT_STRETCH sh) noexcept
{
    assert(face);
    *face = nullptr;
    FontEx key;
    key.font.weight = uint16_t(w);
    key.font.stretch = uint8_t(sh);
    key.font.style = uint8_t(se);
    key.font.length = len;
    if (key.font.length >= FONT_FACE_NAME_LENGTH)
        return CODE_SMALLBUFFER;

    const auto bl = sizeof(char16_t) * key.font.length;
    std::memcpy(key.font.family, name, bl);
    key.font.family[len] = 0;

    key.hash = FONT_HASH::impl(key);
    CISFontFace* fontFace = nullptr;

    // MAPPED
    const auto itr = m_map.find(key);
    if (itr != m_map.end()) {
        itr->second->AddRefCnt();
        *face = itr->second;
        return CODE_OK;
    }
    // CACHED
    for (auto& obj : m_apCached) {
        if (obj) {
            const auto fontData = obj->GetFontData();
            FontEx cachedEx;
            cachedEx.font = *fontData;
            cachedEx.hash = FONT_HASH::impl(cachedEx);
            FONT_EQUL func;
            if (cachedEx.hash == key.hash && func(cachedEx, key)) {
                std::swap(fontFace, obj);
                break;
            }
        }
    }
    // CREATE
    CODE code = CODE_OK;
    if (!fontFace) {
        if (auto obj = new(std::nothrow) CISFontFace{ *this, key.font }) {
            code = obj->Init();
            if (Failure(code)) {
                obj->Dispose();
                obj = nullptr;
            }
            fontFace = obj;
        }
        else
            code = CODE_OUTOFMEMORY;
    }
    // INSERT
    if (fontFace) {
        fontFace->RefCore().position = OBJECT_POS_MAP;
        try {
            m_map.insert({ key, fontFace });
        }
        catch (...) {
            fontFace->Dispose();
            fontFace = nullptr;
            code = CODE_OUTOFMEMORY;
        }
    }
    // OUTPUT
    if (fontFace) {
        *face = fontFace;
    }
    return code;
}

bool CharIS::FreeType::CISFontEngine::TryMove(CISFontFace& obj) noexcept
{
    if (obj.RefCore().position == OBJECT_POS_MAP) {

        FontEx key;
        key.font = *obj.GetFontData();
        key.hash = FONT_HASH::impl(key);
        const auto itr = m_map.find(key);
        if (itr != m_map.end()) {
            m_map.erase(itr);


            assert(m_uRandomIndex < FT_CACHE_QUEUE_LEN);
            auto& cached = m_apCached[m_uRandomIndex];
            this->advIndex();
            CharIS::SafeDispose(cached);
            cached = &obj;
            obj.RefCore().position = OBJECT_POS_CACHE;

            return true;
        }
        else {
            assert(!"CHECK THIS");
        }

    }
    return false;
}

/// <summary>
/// Tses the create text manager.
/// </summary>
/// <param name="mgr">The MGR.</param>
/// <param name="database">The database.</param>
/// <param name="rev">The rev.</param>
/// <returns></returns>
extern "C" CODE CharisCreateFontEngine(IISFontEngine** mgr, IISFontProvider* database, int rev) noexcept
{
    const auto obj = new (std::nothrow) FreeType::CISFontEngine;
    *mgr = nullptr;
    if (obj) {
        const auto code = obj->Init(database);
        if (Failure(code)) {
            obj->Dispose();
            return code;
        }
        *mgr = obj;
        return CODE_OK;
    }
    return CODE_OUTOFMEMORY;
}
