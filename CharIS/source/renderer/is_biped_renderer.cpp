#include "is_biped_renderer.h"
#include "is_text_renderer_impl.h"
#include "../layout/is_text_cell.h"

#include <cstdio>
#include <chrono>

using namespace CharIS;

CharIS::CISBipedRenderer::CISBipedRenderer(CISTextRenderer* p) noexcept
    : m_pParent(p)
{
    assert(p);
}

extern "C" uint32_t*
biped_shrink_size(biped_cache_ctx_t ctx, biped_index_t id, biped_size2d_t size) biped_noexcept;

extern "C" biped_result_t
biped_cache_get_info(biped_cache_ctx_t ctx, biped_index_t index, biped_block_info_t* info) biped_noexcept;

void CharIS::CISBipedRenderer::TaskDone(uint64_t id) noexcept
{
    GlyphTaskId task;
    task.id = id;
    switch (task.gray.type)
    {
    case GLYPH_TASK_TYPE_R:
    {
        const auto ctx = m_ctxBipedR;
        const auto kv = biped_shrink_size(ctx, task.gray.bid, { task.gray.width, task.gray.height });
        const auto bkv = reinterpret_cast<BipedKeyValue*>(kv);
        assert(bkv->value.state == BIPED_STATE_PENDING);
        bkv->value.state = BIPED_STATE_READY;
        bkv->value.offsetX = int16_t(task.gray.offsetX8) << 6;
        bkv->value.offsetY = int16_t(task.gray.offsetY16);
        biped_cache_unlock_id(ctx, &task.gray.bid, 1);
        break;
    }
    case GLYPH_TASK_TYPE_RGB_SUBP:
    {
        const auto ctx = m_ctxBipedRgb;
        const auto kv = biped_shrink_size(ctx, task.gray.bid, { task.gray.width, task.gray.height });
        const auto bkv = reinterpret_cast<BipedKeyValue*>(kv);
        assert(bkv->value.state == BIPED_STATE_PENDING);
        bkv->value.state = BIPED_STATE_READY;
        bkv->value.offsetX = int16_t(task.gray.offsetX8) << 6;
        bkv->value.offsetY = int16_t(task.gray.offsetY16);
        biped_cache_unlock_id(ctx, &task.gray.bid, 1);
        break;
    }
    case GLYPH_TASK_TYPE_RGB_MSDF:
    {
        const auto ctx = m_ctxBipedRgb;
        biped_block_info_t block;
        biped_cache_get_info(ctx, task.msdf.bid, &block);
        const auto bkv = reinterpret_cast<BipedValue*>(block.value);
        assert(bkv->state == BIPED_STATE_PENDING && block.real_size.w == task.msdf.side);
        bkv->state = BIPED_STATE_READY;
        bkv->offsetX = task.msdf.offsetX;
        bkv->offsetY = task.msdf.offsetY;
        biped_cache_unlock_id(ctx, &task.msdf.bid, 1);
        break;
    }
    default:
        assert(!"BAD PATH");
    }

}

void CharIS::CISBipedRenderer::ClearCache(GlyphCache list[], size_t len) noexcept
{
    assert(len <= TEXT_CELL_MAXLEN);
    uint16_t list1[TEXT_CELL_MAXLEN]; 
    uint16_t list2[TEXT_CELL_MAXLEN];
    uint32_t len1 = 0;
    uint32_t len2 = 0;
    for (size_t i = 0; i != len; ++i) {
        const auto data = list[i];
        list[i] = {};
        switch (data.biped.type)
        {
        case GLYPH_CACHE_TYPE_NONE:
            break;
        case GLYPH_CACHE_TYPE_R_GRAY:
            list1[len1++] = data.biped.bid;
            break;
        case GLYPH_CACHE_TYPE_RGB_SUBP:
        case GLYPH_CACHE_TYPE_RGB_MSDF_L:
        case GLYPH_CACHE_TYPE_RGB_MSDF_M:
            list2[len2++] = data.biped.bid;
            break;
        default:
            assert(!"BAD PATH");
            break;
        }
    }

    biped_cache_unlock_id(m_ctxBipedR, list1, len1);
    biped_cache_unlock_id(m_ctxBipedRgb, list2, len2);
}

CODE CharIS::CISBipedRenderer::Init(uint32_t size, bool rgb, bool a) noexcept
{
    assert(m_pGraphicsApi == nullptr);

    //m_uSide = size;

    m_pGraphicsApi = &m_pParent->RefGraphicsApi();
    CreateTextureParam param = {};
    param.side = size;
    param.channel = TEXTURE_CHANNEL_R8;

    CODE code = CODE_OK;

    if (!(rgb || a))
        code = CODE_INVALIDARG;

    if (Success(code) && rgb) {
        m_ctxBipedRgb = biped_cache_create(size, sizeof(BipedKeyValue), sizeof(BipedKey));
        if (!m_ctxBipedRgb)
            code = CODE_INVALIDARG;

        if (Success(code)) {
            param.channel = TEXTURE_CHANNEL_R8G8B8;
            code = m_pGraphicsApi->CreateAltasTexture(param, m_hTextureRgb);
        }

    }

    if (Success(code) && a) {
        m_ctxBipedR = biped_cache_create(size, sizeof(BipedKeyValue), sizeof(BipedKey));
        if (!m_ctxBipedR)
            code = CODE_INVALIDARG;

        if (Success(code)) {
            param.channel = TEXTURE_CHANNEL_R8;
            code = m_pGraphicsApi->CreateAltasTexture(param, m_hTextureR);
        }
    }

    return code;
}

bool CharIS::CISBipedRenderer::DrawCache(GlyphCache cache, const TextEffect& effect, void* context, fp26dot6_t size, fppoint_t point) noexcept
{
    biped_block_info_t block;
    switch (cache.biped.type)
    {
        fp26dot6_t pad;
    case GLYPH_CACHE_TYPE_R_GRAY:
        biped_cache_get_info(m_ctxBipedR, cache.biped.bid, &block);
        return this->draw_glyph_gray(block, size, point, effect, context);
    case GLYPH_CACHE_TYPE_RGB_SUBP:
        biped_cache_get_info(m_ctxBipedRgb, cache.biped.bid, &block);
        return this->draw_glyph_subp(block, size, point, effect, context);
    case GLYPH_CACHE_TYPE_RGB_MSDF_L:
        // L: A. 64/60 B. 68/64 -> 0.4%
        biped_cache_get_info(m_ctxBipedRgb, cache.biped.bid, &block);
        pad = mul(MSDF_PADDING, size);
        point = { point.x - pad, point.y - pad };
        //return this->draw_glyph_msdf(block, mul(size, 68), point, effect, context);
        return this->draw_glyph_msdf(block, mul(size, 64 + MSDF_PADDING * 2), point, effect, context);
    case GLYPH_CACHE_TYPE_RGB_MSDF_M:
        // M: A. 32/28 B. 73/64 -> 0.2%
        biped_cache_get_info(m_ctxBipedRgb, cache.biped.bid, &block);
        pad = mul(MSDF_PADDING * 2, size);
        point = { point.x - pad, point.y - pad };
        return this->draw_glyph_msdf(block, mul(size, 64 + MSDF_PADDING * 4 + 1), point, effect, context);
    }
    return false;
}

GlyphCache CharIS::CISBipedRenderer::CreateCache(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept
{
    //return create_cache_msdf(face, ch);
    //return create_cache_bitmap(BITMAP_TYPE_RGBH, face, ch, size);
    //return create_cache_bitmap(BITMAP_TYPE_RGBV, face, ch, size);
    return create_cache_bitmap(BITMAP_TYPE_GRAY, face, ch, size);

}

GlyphCache CharIS::CISBipedRenderer::create_cache_bitmap(
    BITMAP_TYPE type, IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept
{

    const auto ctx = type == BITMAP_TYPE_GRAY ? m_ctxBipedR : m_ctxBipedRgb;
    const auto cacheType = type == BITMAP_TYPE_GRAY ? GLYPH_CACHE_TYPE_R_GRAY : GLYPH_CACHE_TYPE_RGB_SUBP;

    GlyphCache cache = {};
    // KEY b32-b63
    const auto fontData = face->GetFontData();
    m_sKeyValue.key.fid = m_pParent->FamilyToId(fontData->family);
    m_sKeyValue.key.weight = fontData->weight;
    m_sKeyValue.key.stretch = fontData->stretch;
    m_sKeyValue.key.style = fontData->style;

    // KEY b0-b31
    const uint32_t fsize = std::min<uint32_t>(uint32_t(size) >> 3u, 2047); // fixed 8.3
    m_sKeyValue.key.ch = ch;
    m_sKeyValue.key.size = fsize;

    biped_block_info_t block;
    auto result = biped_cache_lock_key(ctx, reinterpret_cast<uint32_t*>(&m_sKeyValue.key), &block);
    if (biped_is_success(result)) {
        cache.biped.type = cacheType;
        cache.biped.bid = block.id;
    }
    else {
        cache = this->add_task_bitmap(face, type, ctx);
    }
    return cache;
}

#if 0
GlyphCache CharIS::CISBipedRenderer::create_cache_subp(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept
{
    GlyphCache cache = {};
    // KEY b32-b63
    const auto fontData = face->GetFontData();
    m_sKeyValue.key.fid = m_pParent->FamilyToId(fontData->family);
    m_sKeyValue.key.weight = fontData->weight;
    m_sKeyValue.key.stretch = fontData->stretch;
    m_sKeyValue.key.style = fontData->style;

    // KEY b0-b31
    const uint32_t fsize = std::min<uint32_t>(uint32_t(size) >> 3u, 2047); // fixed 8.3
    m_sKeyValue.key.ch = ch;
    m_sKeyValue.key.size = fsize;

    biped_block_info_t block;
    auto result = biped_cache_lock_key(m_ctxBipedRgb, reinterpret_cast<uint32_t*>(&m_sKeyValue.key), &block);
    if (biped_is_success(result)) {
        cache.biped.type = GLYPH_CACHE_TYPE_RGB_SUBP;
        cache.biped.bid = block.id;
    }
    else {
        cache = this->add_task_subp(face);
    }
    return cache;
}
#endif

bool CharIS::CISBipedRenderer::draw_glyph_subp(const biped_block_info_t& block, fp26dot6_t size, fppoint_t point, const TextEffect& effect, void* context) noexcept
{
    const auto bv = reinterpret_cast<BipedValue*>(block.value);
    if (bv->state == BIPED_STATE_READY) {

        if (!(block.real_size.w && block.real_size.h))
            return true;

        DrawTextureParam param = {};
        param.handle = m_hTextureRgb;

        //param.side = m_uSide;
        param.type = TEXT_RENDER_TYPE_SUBPIXEL;
        param.size = size;

        param.dst.point.x = point.x + mul(fp26dot6_t(bv->offsetX), 1 << 6);
        param.dst.point.y = point.y + mul(fp26dot6_t(bv->offsetY), 1 << 6);
        param.dst.size.width = mul(block.real_size.w << 6, 1 << 6);
        param.dst.size.height = mul(block.real_size.h << 6, 1 << 6);

        param.src.x = block.position.x;
        param.src.y = block.position.y;
        param.src.width = block.real_size.w;
        param.src.height = block.real_size.h;

        m_pGraphicsApi->DrawAltas(param, effect, context);
        return true;
    }
    return false;
}


extern "C" static void charis_rgb_to_rgbx_fb(
    const uint8_t* src, uint8_t* dst, uint32_t spitch, uint32_t width, uint32_t height
) noexcept {
    const uint32_t dst_pitch = width * 4;  // dst is always contiguous
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* s = src + static_cast<size_t>(y) * spitch;
        uint8_t* d = dst + static_cast<size_t>(y) * dst_pitch;
        for (uint32_t x = 0; x < width; ++x) {
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = 0xFF;
            s += 3;
            d += 4;
        }
    }
}

extern "C" static void charis_rgbp_to_rgbx_fb(
    const uint8_t* src, uint8_t* dst, uint32_t spitch, uint32_t width, uint32_t height
) noexcept {
    const uint32_t dst_pitch = width * 4;
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* r_row = src + static_cast<size_t>(y * 3 + 0) * spitch;
        const uint8_t* g_row = src + static_cast<size_t>(y * 3 + 1) * spitch;
        const uint8_t* b_row = src + static_cast<size_t>(y * 3 + 2) * spitch;
        uint8_t* d = dst + static_cast<size_t>(y) * dst_pitch;
        for (uint32_t x = 0; x < width; ++x) {
            d[0] = r_row[x];
            d[1] = g_row[x];
            d[2] = b_row[x];
            d[3] = 0xFF;
            d += 4;
        }
    }
}

extern "C" static inline void charis_convert_rgb(
    const void* src, uint8_t* dst, 
    BITMAP_PIXEL_FORAMT format, uint32_t spitch, 
    uint32_t width, uint32_t height
) noexcept {
    // MEMCPY
    if (format == BITMAP_PIXEL_FORAMT_R8) {
        std::memcpy(dst, src, spitch * height);
    }
    // PACKED
    else if (format == BITMAP_PIXEL_FORAMT_RGB8) {
        charis_rgb_to_rgbx_fb(reinterpret_cast<const uint8_t*>(src), dst, spitch, width, height);
    }
    // PLANAR
    else {
        charis_rgbp_to_rgbx_fb(reinterpret_cast<const uint8_t*>(src), dst, spitch, width, height);
    }
}

GlyphCache CharIS::CISBipedRenderer::add_task_bitmap(IISFontFace* face, BITMAP_TYPE type, biped_cache_ctx_t ctx) noexcept
{
    bool rev;
    BITMA_PRENDER_MODE renderMode;
    GLYPH_TASK_TYPE taskType;
    uint64_t handle;
    switch (type)
    {
    default:
    case BITMAP_TYPE_GRAY:
        rev = false;
        renderMode = BITMA_PRENDER_MODE_NORMAL;
        taskType = GLYPH_TASK_TYPE_R;
        handle = m_hTextureR;
        break;
    case BITMAP_TYPE_RGBH:
        renderMode = BITMA_PRENDER_MODE_LCD_HORIZONTAL;
        taskType = GLYPH_TASK_TYPE_RGB_SUBP;
        handle = m_hTextureRgb;
        break;
    case BITMAP_TYPE_BGRH:
        renderMode = BITMA_PRENDER_MODE_LCD_HORIZONTAL;
        taskType = GLYPH_TASK_TYPE_RGB_SUBP;
        handle = m_hTextureRgb;
        rev = true;
        break;
    case BITMAP_TYPE_RGBV:
        renderMode = BITMA_PRENDER_MODE_LCD_VERTICAL;
        taskType = GLYPH_TASK_TYPE_RGB_SUBP;
        handle = m_hTextureRgb;
        break;
    case BITMAP_TYPE_BGRV:
        renderMode = BITMA_PRENDER_MODE_LCD_VERTICAL;
        rev = true;
        taskType = GLYPH_TASK_TYPE_RGB_SUBP;
        handle = m_hTextureRgb;
        break;
    }

    char32_t ch = m_sKeyValue.key.ch;
    fp26dot6_t size = m_sKeyValue.key.size << 3u;
    CODE code = CODE_OK;
    GlyphMetrics gm;
    IISFontFaceGlyph* glyph = nullptr;
    if (Success(code)) {
        code = face->Lock(ch, &glyph);
    }
    if (Success(code)) {
        glyph->GetGlyphMetrics(&gm);
        face->Unlock(glyph);
        glyph = nullptr;
    }
    else {
        CHARIS_LOGGER_ERR("add_biped_task lock failed: %08X\n", code);
        return {};
    }


    const int64_t units = FONT_METRICS_UNIT; // face->GetFontMetrics()->units;

#if 1
    // Font hinting (ON)
    constexpr fp26dot6_t hintngH = (1 << 6) / 2;
    //constexpr fp26dot6_t hintngH = 1;
    //constexpr fp26dot6_t hintngV = (1 << 6) - 2;
    //constexpr fp26dot6_t hintngV = (1 << 6) * 4 / 3 - 2;
    constexpr fp26dot6_t hintngV = (1 << 6) - 2;
#else
    // Font hinting (OFF)
    constexpr fp26dot6_t hintngH = 0;
    constexpr fp26dot6_t hintngV = 0;
#endif

    const auto empty1 = gm.advanceWidth - gm.leftSideBearing - gm.rightSideBearing;
    const auto empty2 = gm.advanceHeight - gm.topSideBearing - gm.bottomSideBearing;
    m_sKeyValue.value.state = empty1 && empty2 ? BIPED_STATE_PENDING : BIPED_STATE_READY;

    const fp26dot6_t leftUnit = gm.leftSideBearing;
    const fp26dot6_t rightUnit = gm.advanceWidth - gm.rightSideBearing;
    const fp26dot6_t leftPixel = fp26dot6_t(int64_t(size) * int64_t(leftUnit) / units);
    const fp26dot6_t rightPixel = fp26dot6_t(int64_t(size) * int64_t(rightUnit) / units);
    const fp26dot6_t widthPixel = ceil(rightPixel + hintngH) - floor(leftPixel - hintngH);

    const fp26dot6_t topUnit = gm.topSideBearing;
    const fp26dot6_t bottomUnit = gm.advanceHeight - gm.bottomSideBearing;
    const fp26dot6_t topPixel = fp26dot6_t(int64_t(size) * int64_t(topUnit) / units);
    const fp26dot6_t bottomPixel = fp26dot6_t(int64_t(size) * int64_t(bottomUnit) / units);
    const fp26dot6_t HeightPixel = ceil(bottomPixel + hintngV) - floor(topPixel - hintngV);

    const int32_t widthPixelApproximate = widthPixel >> 6;
    const int32_t HeightPixelApproximate = HeightPixel >> 6;

    const biped_size2d_t pixelSize = { biped_unit_t(widthPixelApproximate), biped_unit_t(HeightPixelApproximate) };
    biped_block_info_t block;
    auto result = biped_cache_lock_key_value(ctx, pixelSize, reinterpret_cast<uint32_t*>(&m_sKeyValue), &block);
    if (!biped_is_success(result)) {
        CHARIS_LOGGER_ERR("add_biped_task_msdf lock failed, required");
        return {};
    }

    if (m_sKeyValue.value.state == BIPED_STATE_READY) {
        GlyphCache cache = {};
        cache.biped.type = type == BITMAP_TYPE_GRAY ? GLYPH_CACHE_TYPE_R_GRAY : GLYPH_CACHE_TYPE_RGB_SUBP;
        cache.biped.bid = block.id;
        return cache;
    }


    base_ptr<IISFontFace> face_ptr;

    *face_ptr.as_get() = face;
    face->AddRefCnt();

    const auto graphics = m_pGraphicsApi;
    const auto position = block.position;
    const auto real_size = block.real_size;
    const auto id = block.id;

    m_pParent->RefThreadPool().Add([=, face_ptr = std::move(face_ptr)]() noexcept {
        BitmapInfo bmp;
        uint8_t* buffer = nullptr;
        IISFontFaceGlyph* glyph = nullptr;
        auto code = face_ptr->Lock(ch, &glyph);
        const int32_t descent = 0;
        uint32_t pitch = 1;
        if (Success(code)) {
            code = glyph->RenderToBitmap(size, &bmp, renderMode);
        }
        if (Success(code)) {

            if (bmp.width > real_size.w || bmp.height > real_size.h) {
                CHARIS_LOGGER_ERR("Approximate(Font Hinting) failed @ %x [%d %d] -> [%d %d]\n", int(ch), real_size.w, real_size.h, bmp.width, bmp.height);
            }

            bmp.width = std::min<uint32_t>(bmp.width, real_size.w);
            bmp.height = std::min<uint32_t>(bmp.height, real_size.h);

            pitch = bmp.format == BITMAP_PIXEL_FORAMT_R8 ? bmp.pitch : bmp.width*4;
            const size_t len = size_t(pitch) * size_t(bmp.height);

            buffer = reinterpret_cast<uint8_t*>(charis_malloc(len));
            if (buffer) {
                charis_convert_rgb(bmp.data, buffer, bmp.format, bmp.pitch, bmp.width, bmp.height);
            }

            // Get font metrics to calculate descent offset (same as MSDF mode)
            const auto fm = face_ptr->GetFontMetrics();
            //descent = fm->descent;
        }
        if (glyph) {
            face_ptr->Unlock(glyph);
            glyph = nullptr;
        }

        if (buffer) {
            Box2D box;
            box.x = position.x;
            box.y = position.y;
            box.width = bmp.width;
            box.height = bmp.height;

            GlyphTaskId task = {};
            task.gray.type = taskType;
            task.gray.bid = id;
            task.gray.width = bmp.width;
            task.gray.height = bmp.height;
            const auto offsetX = bmp.offsetX;
            // Calculate descent in pixels: descent * size / FONT_METRICS_UNIT
            const int64_t units = FONT_METRICS_UNIT;
            const int32_t descentPixel = int32_t(int64_t(descent) * int64_t(size) / units);
            // Subtract descent to match MSDF mode behavior
            const auto offsetY = size - (bmp.offsetY << 6) - descentPixel;
            task.gray.offsetX8 = offsetX;
            task.gray.offsetY16 = offsetY;
            graphics->Upload(handle, task.id, box, buffer, pitch);

            charis_free(buffer);
        }
    });

    return {};
}

#if 0
GlyphCache CharIS::CISBipedRenderer::add_task_subp(IISFontFace* face) noexcept
{
    char32_t ch = m_sKeyValue.key.ch;
    fp26dot6_t size = m_sKeyValue.key.size << 3u;
    CODE code = CODE_OK;
    GlyphMetrics gm;
    IISFontFaceGlyph* glyph = nullptr;
    if (Success(code)) {
        code = face->Lock(ch, &glyph);
    }
    if (Success(code)) {
        glyph->GetGlyphMetrics(&gm);
        face->Unlock(glyph);
        glyph = nullptr;
    }
    else {
        CHARIS_LOGGER_ERR("add_biped_task lock failed: %08X\n", code);
        return {};
    }


    const int64_t units = FONT_METRICS_UNIT; // face->GetFontMetrics()->units;

#if 1
    // Font hinting (ON)
    constexpr fp26dot6_t hintngH = (1 << 6) / 3;
    //constexpr fp26dot6_t hintngV = (1 << 6) - 2;
    constexpr fp26dot6_t hintngV = (1 << 6) * 4 / 3 - 2;
#else
    // Font hinting (OFF)
    constexpr fp26dot6_t hintng= 0;
#endif

    const auto empty1 = gm.advanceWidth - gm.leftSideBearing - gm.rightSideBearing;
    const auto empty2 = gm.advanceHeight - gm.topSideBearing - gm.bottomSideBearing;
    m_sKeyValue.value.state = empty1 && empty2 ? BIPED_STATE_PENDING : BIPED_STATE_READY;

    const fp26dot6_t leftUnit = gm.leftSideBearing;
    const fp26dot6_t rightUnit = gm.advanceWidth - gm.rightSideBearing;
    const fp26dot6_t leftPixel = fp26dot6_t(int64_t(size) * int64_t(leftUnit) / units);
    const fp26dot6_t rightPixel = fp26dot6_t(int64_t(size) * int64_t(rightUnit) / units);
    const fp26dot6_t widthPixel = ceil(rightPixel + hintngH) - floor(leftPixel - hintngH);

    const fp26dot6_t topUnit = gm.topSideBearing;
    const fp26dot6_t bottomUnit = gm.advanceHeight - gm.bottomSideBearing;
    const fp26dot6_t topPixel = fp26dot6_t(int64_t(size) * int64_t(topUnit) / units);
    const fp26dot6_t bottomPixel = fp26dot6_t(int64_t(size) * int64_t(bottomUnit) / units);
    const fp26dot6_t HeightPixel = ceil(bottomPixel + hintngV) - floor(topPixel - hintngV);

    const int32_t widthPixelApproximate = widthPixel >> 6;
    const int32_t HeightPixelApproximate = HeightPixel >> 6;

    const biped_size2d_t pixelSize = { biped_unit_t(widthPixelApproximate), biped_unit_t(HeightPixelApproximate) };
    biped_block_info_t block;
    auto result = biped_cache_lock_key_value(m_ctxBipedRgb, pixelSize, reinterpret_cast<uint32_t*>(&m_sKeyValue), &block);
    if (!biped_is_success(result)) {
        CHARIS_LOGGER_ERR("add_biped_task_msdf lock failed, required");
        return {};
    }

    if (m_sKeyValue.value.state == BIPED_STATE_READY) {
        GlyphCache cache = {};
        cache.biped.type = GLYPH_CACHE_TYPE_RGB_SUBP;
        cache.biped.bid = block.id;
        return cache;
    }


    base_ptr<IISFontFace> face_ptr;

    *face_ptr.as_get() = face;
    face->AddRefCnt();

    const auto graphics = m_pGraphicsApi;
    const auto handle = m_hTextureRgb;
    const auto position = block.position;
    const auto real_size = block.real_size;
    const auto id = block.id;

    m_pParent->RefThreadPool().Add([=, face_ptr = std::move(face_ptr)]() noexcept {
        BitmapInfo bmp;
        uint8_t* buffer = nullptr;
        IISFontFaceGlyph* glyph = nullptr;
        auto code = face_ptr->Lock(ch, &glyph);
        const int32_t descent = 0;
        if (Success(code)) {
            //code = glyph->RenderToBitmap(size, &bmp, BITMA_PRENDER_MODE_LCD_HORIZONTAL);
            code = glyph->RenderToBitmap(size, &bmp, BITMA_PRENDER_MODE_LCD_VERTICAL);
        }
        if (Success(code)) {
            assert(bmp.format != BITMAP_PIXEL_FORAMT_R8 && "SUBPIXEL");

            if (bmp.width > real_size.w || bmp.height > real_size.h) {
                CHARIS_LOGGER_ERR("Approximate(Font Hinting) failed @ %x [%d %d] -> [%d %d]\n", int(ch), real_size.w, real_size.h, bmp.width, bmp.height);
            }

            bmp.width = std::min<uint32_t>(bmp.width, real_size.w);
            bmp.height = std::min<uint32_t>(bmp.height, real_size.h);
            const size_t len = size_t(bmp.width) * size_t(bmp.height) * 4;

            buffer = reinterpret_cast<uint8_t*>(charis_malloc(len));
            if (buffer) {
                charis_convert_rgb(bmp.data, buffer, bmp.format, bmp.pitch, bmp.width, bmp.height);
            }

            // Get font metrics to calculate descent offset (same as MSDF mode)
            const auto fm = face_ptr->GetFontMetrics();
            //descent = fm->descent;
        }
        if (glyph) {
            face_ptr->Unlock(glyph);
            glyph = nullptr;
        }

        if (buffer) {
            Box2D box;
            box.x = position.x;
            box.y = position.y;
            box.width = bmp.width;
            box.height = bmp.height;

            GlyphTaskId task = {};
            task.gray.type = GLYPH_TASK_TYPE_RGB_SUBP;
            task.gray.bid = id;
            task.gray.width = bmp.width;
            task.gray.height = bmp.height;
            const auto offsetX = bmp.offsetX;
            // Calculate descent in pixels: descent * size / FONT_METRICS_UNIT
            const int64_t units = FONT_METRICS_UNIT;
            const int32_t descentPixel = int32_t(int64_t(descent) * int64_t(size) / units);
            // Subtract descent to match MSDF mode behavior
            const auto offsetY = size - (bmp.offsetY << 6) - descentPixel;
            task.gray.offsetX8 = offsetX;
            task.gray.offsetY16 = offsetY;
            graphics->Upload(handle, task.id, box, buffer, bmp.width * 4);

            charis_free(buffer);
        }
        });

    return {};
}
#endif

bool CharIS::CISBipedRenderer::draw_glyph_gray(const biped_block_info_t& block, fp26dot6_t size, fppoint_t point, const TextEffect& effect, void* context) noexcept
{
    const auto bv = reinterpret_cast<BipedValue*>(block.value);
    if (bv->state == BIPED_STATE_READY) {

        if (!(block.real_size.w && block.real_size.h))
            return true;

        DrawTextureParam param = {};
        param.handle = m_hTextureR;

        //param.side = m_uSide;
        param.type = TEXT_RENDER_TYPE_GRAY;
        param.size = size;

        param.dst.point.x = point.x + mul(fp26dot6_t(bv->offsetX), 1 << 6);
        param.dst.point.y = point.y + mul(fp26dot6_t(bv->offsetY), 1 << 6);
        param.dst.size.width = mul(block.real_size.w << 6, 1 << 6);
        param.dst.size.height = mul(block.real_size.h << 6, 1 << 6);

        param.src.x = block.position.x;
        param.src.y = block.position.y;
        param.src.width = block.real_size.w;
        param.src.height = block.real_size.h;

        m_pGraphicsApi->DrawAltas(param, effect, context);
        return true;
    }
    return false;
}

#if 0
GlyphCache CharIS::CISBipedRenderer::create_cache_gray(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept
{
    GlyphCache cache = {};
    // KEY b32-b63
    const auto fontData = face->GetFontData();
    m_sKeyValue.key.fid = m_pParent->FamilyToId(fontData->family);
    m_sKeyValue.key.weight = fontData->weight;
    m_sKeyValue.key.stretch = fontData->stretch;
    m_sKeyValue.key.style = fontData->style;

    // KEY b0-b31
    const uint32_t fsize = std::min<uint32_t>(uint32_t(size) >> 3u, 2047); // fixed 8.3
    m_sKeyValue.key.ch = ch;
    m_sKeyValue.key.size = fsize;

    biped_block_info_t block;
    auto result = biped_cache_lock_key(m_ctxBipedR, reinterpret_cast<uint32_t*>(&m_sKeyValue.key), &block);
    if (biped_is_success(result)) {
        cache.biped.type = GLYPH_CACHE_TYPE_R_GRAY;
        cache.biped.bid = block.id;
    }
    else {
        cache = this->add_task_gray(face);
    }
    return cache;
}


GlyphCache CharIS::CISBipedRenderer::add_task_gray(IISFontFace* face) noexcept
{
    char32_t ch = m_sKeyValue.key.ch;
    fp26dot6_t size = m_sKeyValue.key.size << 3u;
    CODE code = CODE_OK;
    GlyphMetrics gm;
    IISFontFaceGlyph* glyph = nullptr;
    if (Success(code)) {
        code = face->Lock(ch, &glyph);
    }
    if (Success(code)) {
        glyph->GetGlyphMetrics(&gm);
        face->Unlock(glyph);
        glyph = nullptr;
    }
    else {
        CHARIS_LOGGER_ERR("add_biped_task lock failed: %08X\n", code);
        return {};
    }


    const int64_t units = FONT_METRICS_UNIT; // face->GetFontMetrics()->units;

#if 1
    // Font hinting (ON)
    //constexpr fp26dot6_t hintng = (1 << 6) - 1;
    constexpr fp26dot6_t hintng = (1 << 6) - 2;
#else
    // Font hinting (OFF)
    constexpr fp26dot6_t hintng = 0;
#endif

    const auto empty1 = gm.advanceWidth - gm.leftSideBearing - gm.rightSideBearing;
    const auto empty2 = gm.advanceHeight - gm.topSideBearing - gm.bottomSideBearing;
    m_sKeyValue.value.state = empty1 && empty2 ? BIPED_STATE_PENDING : BIPED_STATE_READY;

    const fp26dot6_t leftUnit = gm.leftSideBearing;
    const fp26dot6_t rightUnit = gm.advanceWidth - gm.rightSideBearing;
    const fp26dot6_t leftPixel = fp26dot6_t(int64_t(size) * int64_t(leftUnit) / units);
    const fp26dot6_t rightPixel = fp26dot6_t(int64_t(size) * int64_t(rightUnit) / units);
    const fp26dot6_t widthPixel = ceil(rightPixel + hintng) - floor(leftPixel - hintng);

    const fp26dot6_t topUnit = gm.topSideBearing;
    const fp26dot6_t bottomUnit = gm.advanceHeight - gm.bottomSideBearing;
    const fp26dot6_t topPixel = fp26dot6_t(int64_t(size) * int64_t(topUnit) / units);
    const fp26dot6_t bottomPixel = fp26dot6_t(int64_t(size) * int64_t(bottomUnit) / units);
    const fp26dot6_t HeightPixel = ceil(bottomPixel + hintng) - floor(topPixel - hintng);

    const int32_t widthPixelApproximate = widthPixel >> 6;
    const int32_t HeightPixelApproximate = HeightPixel >> 6;

    const biped_size2d_t pixelSize = { biped_unit_t(widthPixelApproximate), biped_unit_t(HeightPixelApproximate) };
    biped_block_info_t block;
    auto result = biped_cache_lock_key_value(m_ctxBipedR, pixelSize, reinterpret_cast<uint32_t*>(&m_sKeyValue), &block);
    if (!biped_is_success(result)) {
        CHARIS_LOGGER_ERR("add_biped_task_msdf lock failed, required");
        return {};
    }

    if (m_sKeyValue.value.state == BIPED_STATE_READY) {
        GlyphCache cache = {};
        cache.biped.type = GLYPH_CACHE_TYPE_R_GRAY;
        cache.biped.bid = block.id;
        return cache;
    }


    base_ptr<IISFontFace> face_ptr;

    *face_ptr.as_get() = face;
    face->AddRefCnt();

    const auto graphics = m_pGraphicsApi;
    const auto handle = m_hTextureR;
    const auto position = block.position;
    const auto real_size = block.real_size;
    const auto id = block.id;

    m_pParent->RefThreadPool().Add([=,face_ptr=std::move(face_ptr)]() noexcept {
        BitmapInfo bmp;
        uint8_t* buffer = nullptr;
        IISFontFaceGlyph* glyph = nullptr;
        auto code = face_ptr->Lock(ch, &glyph);
        const int32_t descent = 0;
        if (Success(code)) {
            code = glyph->RenderToBitmap(size, &bmp, BITMA_PRENDER_MODE_NORMAL);
        }
        if (Success(code)) {
            assert(bmp.format == BITMAP_PIXEL_FORAMT_R8 && "TODO: SUBPIXEL");
            
            if (bmp.width > real_size.w || bmp.height > real_size.h) {
                CHARIS_LOGGER_ERR("Approximate(Font Hinting) failed @ %x [%d %d] -> [%d %d]\n", int(ch), real_size.w, real_size.h, bmp.width, bmp.height);
            }

            bmp.width = std::min<uint32_t>(bmp.width, real_size.w);
            bmp.height = std::min<uint32_t>(bmp.height, real_size.h);
            const size_t len = size_t(bmp.pitch) * size_t(bmp.height);

            buffer = reinterpret_cast<uint8_t*>(charis_malloc(len));
            if (buffer) {
                std::memcpy(buffer, bmp.data, len);
            }

            // Get font metrics to calculate descent offset (same as MSDF mode)
            const auto fm = face_ptr->GetFontMetrics();
            //descent = fm->descent;
        }
        if (glyph) {
            face_ptr->Unlock(glyph);
            glyph = nullptr;
        }

        if (buffer) {
            Box2D box;
            box.x = position.x;
            box.y = position.y;
            box.width = bmp.width;
            box.height = bmp.height;

            GlyphTaskId task = {};
            task.gray.type = GLYPH_TASK_TYPE_R;
            task.gray.bid = id;
            task.gray.width = bmp.width;
            task.gray.height = bmp.height;
            const auto offsetX = bmp.offsetX;
            // Calculate descent in pixels: descent * size / FONT_METRICS_UNIT
            const int64_t units = FONT_METRICS_UNIT;
            const int32_t descentPixel = int32_t(int64_t(descent) * int64_t(size) / units);
            // Subtract descent to match MSDF mode behavior
            const auto offsetY = size  - (bmp.offsetY << 6) - descentPixel;
            task.gray.offsetX8 = offsetX;
            task.gray.offsetY16 = offsetY;
            graphics->Upload(handle, task.id, box, buffer, bmp.pitch);

            charis_free(buffer);
        }
    });

    return {};
}
#endif

CharIS::CISBipedRenderer::~CISBipedRenderer() noexcept
{
    if (m_hTextureR) {
        m_pGraphicsApi->DisposeTexture(Take(m_hTextureR));
    }
    if (m_hTextureRgb) {
        m_pGraphicsApi->DisposeTexture(Take(m_hTextureRgb));
    }
    biped_cache_dispose(m_ctxBipedRgb);
    biped_cache_dispose(m_ctxBipedR);
}
