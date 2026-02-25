#include "is_biped_renderer.h"
#include "is_text_renderer_impl.h"

//#include <msdfgen.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#define MSDFCC_C_IMPLEMENTATION
#define MSDFCC_USE_FLOAT
#include "msdfcc.h"
#include "../core/is_helper_c.h"

using namespace CharIS;

extern "C"
static uint16_t StandAloneComplex(
    const GlyphMetrics* glyph,
    const FontMetrics* metrics,
    const CharIS::Font* font
) noexcept {
    // Evaluate glyph complexity
    const uint32_t p = glyph->points;
    const uint32_t c = glyph->contours;
    if (p) {
        const uint32_t score = p + c * 10;
        //return MSDF_SIZE_M;
        return score > 60 || metrics->weiget < FONT_WEIGHT_NORMAL ? MSDF_SIZE_L : MSDF_SIZE_M;
    }
    return MSDF_SIZE_Z;
}

struct CISMsdfOutlineSink : IISGeometrySink {

    CISMsdfOutlineSink(uint32_t len) {
        msdfcc_shape = msdfcc_shape_create(len);
    }

    ~CISMsdfOutlineSink() {
        msdfcc_shape_dispose(msdfcc_shape);
    }
#if 0
    

    msdfgen::Point2         position;
    msdfgen::Shape          shape;
    msdfgen::Contour*       contour = nullptr;

    static inline auto msdf(const PointF vector) noexcept {
        return msdfgen::Point2{ vector.x , vector.y };
    }

    void move_to_impl(PointF to) {
        if (!(contour && contour->edges.empty()))
            contour = &shape.addContour();
        position = msdf(to);
    }

    void line_to_impl(PointF to) {
        const auto endpoint = msdf(to);
        if (endpoint != position) {
            contour->addEdge({ position, endpoint });
            position = endpoint;
        }
    };

    void conic_to_impl(PointF control, PointF to) {
        const auto endpoint = msdf(to);
        if (endpoint != position) {
            contour->addEdge({ position, msdf(control), endpoint });
            position = endpoint;
        }
    };

    void cubic_to_impl(PointF control1, PointF control2, PointF to) {
        const auto endpoint = msdf(to);
        const auto c1 = msdf(control1);
        const auto c2 = msdf(control2);
        if (endpoint != position || msdfgen::crossProduct(c1 - endpoint, c2 - endpoint)) {
            contour->addEdge({ position, c1, c2, endpoint });
            position = endpoint;
        }
    };
#endif
    static inline auto msdfcc(const PointF vector) noexcept {
        return msdfcc_point_t{ (msdfcc_float_t)vector.x , (msdfcc_float_t)vector.y };
    }

    void MoveTo(PointF to) noexcept override {
        {
            msdfcc_contour_index = msdfcc_shape->length;
            msdfcc_shape = msdfcc_shape_new_contour(msdfcc_shape);
            last = msdfcc(to);
        }

        //this->move_to_impl(to);
    }

    void LineTo(PointF to) noexcept override {
        {
            const auto endpoint = msdfcc(to);
            if (!msdfcc_points_equal(endpoint, last)) {
                auto& contour = msdfcc_shape->contours[msdfcc_contour_index];
                msdfcc_edge_t edge;
                contour = msdfcc_contour_add_segment(
                    contour, msdfcc_edge_linear(&edge, last, endpoint)
                );
                last = endpoint;
            }
        }
        //this->line_to_impl(to);
    };

    void ConicTo(PointF control, PointF to) noexcept override {
        {
            const auto endpoint = msdfcc(to);
            if (!msdfcc_points_equal(endpoint, last)) {
                auto& contour = msdfcc_shape->contours[msdfcc_contour_index];
                msdfcc_edge_t edge;
                contour = msdfcc_contour_add_segment(
                    contour, msdfcc_edge_quadratic(&edge, last, msdfcc(control), endpoint)
                );
                last = endpoint;
            }
        }
        //this->conic_to_impl(control, to);
    };

    void CubicTo(PointF control1, PointF control2, PointF to) noexcept override {
        {
            const auto endpoint = msdfcc(to);
            const auto c1 = msdfcc(control1);
            const auto c2 = msdfcc(control2);
            msdfcc_point_t v1 = { c1.x - endpoint.x, c1.y - endpoint.y };
            msdfcc_point_t v2 = { c2.x - endpoint.x, c2.y - endpoint.y };
            if (!msdfcc_points_equal(endpoint, last) || msdfcc_points_cross(v1, v2) != 0) {
                auto& contour = msdfcc_shape->contours[msdfcc_contour_index];
                msdfcc_edge_t edge;
                contour = msdfcc_contour_add_segment(
                    contour, msdfcc_edge_cubic(&edge, last, c1, c2, endpoint)
                );
                last = endpoint;
            }
        }
        //this->cubic_to_impl(control1, control2, to);
    };

    msdfcc_point_t          last = {};
    msdfcc_shape_t*         msdfcc_shape = nullptr;
    uint32_t                msdfcc_contour_index = 0;

    bool Check() noexcept { 
        const auto count = msdfcc_shape->length;
        for (uint32_t i = 0; i != count; ++i) {
            const auto c = msdfcc_shape->contours[i];
            if (!c)
                return false;
        }
        return true;
    }
};

#if defined(CHARIS_SIMD_SSE2)
#include <emmintrin.h>
#endif
#if defined(CHARIS_SIMD_NEON)
#include <arm_neon.h>
#endif

namespace CharIS { namespace impl {

    void msdf_normalization_rgba_fb(
        uint8_t* output, const float input[],
        size_t size
    ) noexcept {
        constexpr size_t unit = 4;

        for (size_t y = 0; y != size; ++y) {
            const auto offsetO = output + (size - 1 - y) * unit * size;
            const auto offsetI = input + y * unit * size;

            for (size_t x = 0; x != size; ++x) {
                offsetO[x * unit + 0] = msdfcc_float2byte(offsetI[x * unit + 0]);
                offsetO[x * unit + 1] = msdfcc_float2byte(offsetI[x * unit + 1]);
                offsetO[x * unit + 2] = msdfcc_float2byte(offsetI[x * unit + 2]);
                offsetO[x * unit + 3] = msdfcc_float2byte(offsetI[x * unit + 3]);
            }
        }
    }

#if defined(CHARIS_SIMD_SSE2)
    void msdf_normalization_rgba_sse2(
        uint8_t* output, const float input[],
        size_t size
    ) noexcept {
        constexpr size_t unit = 4;
        const __m128 zero = _mm_setzero_ps();
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 scale = _mm_set1_ps(255.0f);
        const __m128 half = _mm_set1_ps(0.5f);

        for (size_t y = 0; y != size; ++y) {
            uint8_t* offsetO = output + (size - 1 - y) * unit * size;
            const float* offsetI = input + y * unit * size;

            for (size_t x = 0; x != size; x += 2) {
                __m128 a, b;
#if defined(CHARIS_SIMD_INPUT_ALIGNED)
                a = _mm_load_ps(offsetI + (x + 0) * unit);
                b = _mm_load_ps(offsetI + (x + 1) * unit);
#else
                a = _mm_loadu_ps(offsetI + (x + 0) * unit);
                b = _mm_loadu_ps(offsetI + (x + 1) * unit);
#endif
                a = _mm_min_ps(_mm_max_ps(a, zero), one);
                b = _mm_min_ps(_mm_max_ps(b, zero), one);
                a = _mm_add_ps(_mm_mul_ps(a, scale), half);
                b = _mm_add_ps(_mm_mul_ps(b, scale), half);
                __m128i ia = _mm_cvtps_epi32(a);
                __m128i ib = _mm_cvtps_epi32(b);
                __m128i i16 = _mm_packs_epi32(ia, ib);
                __m128i u8 = _mm_packus_epi16(i16, i16);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(offsetO + x * unit), u8);
            }
        }
    }
#endif

#if defined(CHARIS_SIMD_NEON)
    void msdf_normalization_rgba_neon(
        uint8_t* output, const float input[],
        size_t size
    ) noexcept {
        constexpr size_t unit = 4;
        const float32x4_t zero = vdupq_n_f32(0.0f);
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t scale = vdupq_n_f32(255.0f);
        const float32x4_t half = vdupq_n_f32(0.5f);

        for (size_t y = 0; y != size; ++y) {
            uint8_t* offsetO = output + (size - 1 - y) * unit * size;
            const float* offsetI = input + y * unit * size;

            for (size_t x = 0; x != size; x += 2) {
                float32x4_t a, b;
                a = vld1q_f32(offsetI + (x + 0) * unit);
                b = vld1q_f32(offsetI + (x + 1) * unit);
                a = vminq_f32(vmaxq_f32(a, zero), one);
                b = vminq_f32(vmaxq_f32(b, zero), one);
                a = vaddq_f32(vmulq_f32(a, scale), half);
                b = vaddq_f32(vmulq_f32(b, scale), half);
                int32x4_t ia = vcvtq_s32_f32(a);
                int32x4_t ib = vcvtq_s32_f32(b);
                int16x4_t i16a = vqmovn_s32(ia);
                int16x4_t i16b = vqmovn_s32(ib);
                int16x8_t i16 = vcombine_s16(i16a, i16b);
                uint8x8_t u8 = vqmovun_s16(i16);
                vst1_u8(offsetO + x * unit, u8);
            }
        }
    }
#endif

    void msdf_normalization_rgba(
        uint8_t* output, const float input[],
        size_t size
    ) noexcept {
#if defined(CHARIS_SIMD_SSE2)
        msdf_normalization_rgba_sse2(output, input, size);
#elif defined(CHARIS_SIMD_NEON)
        msdf_normalization_rgba_neon(output, input, size);
#else
        msdf_normalization_rgba_fb(output, input, size);
#endif
    }

    void msdf_safe_zone_rgba(
        uint8_t* output,
        size_t size
    ) noexcept {
        constexpr size_t unit = 4;

        uint8_t* const topRow = output;
        std::memset(topRow, 0, size * unit);

        uint8_t* const bottomRow = output + (size - 1) * unit * size;
        std::memset(bottomRow, 0, size * unit);

        for (size_t y = 1; y < size - 1; ++y) {
            uint8_t* const leftPixel = output + (size - 1 - y) * unit * size;
            *reinterpret_cast<uint32_t*>(leftPixel) = 0;

            uint8_t* const rightPixel = output + (size - 1 - y) * unit * size + (size - 1) * unit;
            *reinterpret_cast<uint32_t*>(rightPixel) = 0;
        }
    }

}}
#if 0
void assert_sharp(const msdfgen::Shape* msdfgen, const msdfcc_shape_t* msdfcc) {
    assert(msdfgen != nullptr && msdfcc != nullptr);
    const size_t n_contours = msdfgen->contours.size();
    assert(n_contours == msdfcc->length);

    for (size_t ci = 0; ci < n_contours; ++ci) {
        const auto& mg_contour = msdfgen->contours[ci];
        const msdfcc_contour_t* mc_contour = msdfcc->contours[ci];
        assert(mc_contour != nullptr);

        const size_t n_edges = mg_contour.edges.size();
        assert(n_edges == mc_contour->length);

        for (size_t ei = 0; ei < n_edges; ++ei) {
            const msdfgen::EdgeSegment* mg_edge = mg_contour.edges[ei];
            assert(mg_edge != nullptr);
            const msdfcc_edge_t* mc_edge = &mc_contour->segments[ei];

            const int mg_type = mg_edge->type();
            assert(mg_type >= 1 && mg_type <= 3);
            assert((int)mc_edge->type == mg_type);

            assert((int)mg_edge->color == (int)mc_edge->color);

            const msdfgen::Point2* mg_cp = mg_edge->controlPoints();
            const int n_cp = mg_type + 1;  /* linear:2, quadratic:3, cubic:4 */
            for (int i = 0; i < n_cp; ++i) {
                assert(mg_cp[i].x == mc_edge->p[i].x && mg_cp[i].y == mc_edge->p[i].y);
            }
        }
    }
}
#endif

extern "C" static inline void charis_create_msdf(
    IISGraphics* api,
    biped_point2d_t point,
    biped_size2d_t size,
    uint64_t handle,
    IISFontFace* face,
    char32_t ch,
    uint16_t id,
    uint16_t msdfSize,
    void * buffer) noexcept
{
    int32_t glyphWidth;
    int32_t glyphHeight;
    const int32_t descent = 0;
    int32_t offx = 0;
    int32_t offy = 0;
    GlyphMetrics glyph;
    CODE code = CODE_OK;

    IISFontFaceGlyph* glyph_ptr = nullptr;
    code = face->Lock(ch, &glyph_ptr);
    if (Failure(code))
        return;

    glyph_ptr->GetGlyphMetrics(&glyph);

    double baseScale = 1.;

    CISMsdfOutlineSink sink{ glyph.contours };
    if (sink.msdfcc_shape && glyph_ptr) {
        glyphWidth = glyph.advanceWidth - glyph.leftSideBearing - glyph.rightSideBearing;
        glyphHeight = glyph.advanceHeight - glyph.topSideBearing - glyph.bottomSideBearing;
        assert(glyphWidth >= 0 && glyphHeight >= 0);
        const auto fm = face->GetFontMetrics();
        const auto units = double(FONT_METRICS_UNIT);
        if (glyphWidth && glyphHeight) {
            const double glyphScaleX = units / double(glyphWidth);
            const double glyphScaleY = units / double(glyphHeight);
            const double glyphScaleBase = std::min(glyphScaleX, glyphScaleY);
            if (glyphScaleBase > 1) {
                //glyphScale = std::min(glyphScaleBase * 0.9, 3.0);
                //baseScale = std::min(glyphScaleBase, 1.5);;
            }
            code = glyph_ptr->GetOutline(&sink, msdfSize * baseScale);
        }
    }
    if (glyph_ptr) {
        face->Unlock(glyph_ptr);
        glyph_ptr = nullptr;
    }
    if (!sink.msdfcc_shape)
        return;

    GlyphTaskId task = {};

    task.gray.type = GLYPH_TASK_TYPE_RGB_MSDF;

    constexpr int32_t unitsPerEm = FONT_METRICS_UNIT;
    msdfcc_rgba_t* bitmap = (msdfcc_rgba_t*)charis_malloc(sizeof(msdfcc_rgba_t) * size_t(msdfSize) * size_t(msdfSize));

    if (glyphWidth && glyphHeight && bitmap && sink.Check()) {
        const int32_t x2 = glyph.advanceWidth + glyph.leftSideBearing - glyph.rightSideBearing;
        const double offsetx = double(unitsPerEm) / baseScale - double(x2);
        const int32_t h = glyphHeight;
        const int32_t by = glyph.verticalOriginY - glyph.topSideBearing;
        const int32_t y2 = 2 * by - h;
        const double offsety = double(unitsPerEm) / baseScale - double(y2);
        offx = (x2 - unitsPerEm) / 2;
        offy = (unitsPerEm - y2) / 2 - descent;

        assert(-10000 <= offx && offx <= 10000);
        assert(-10000 <= offy && offy <= 10000);


        const double target = msdfSize - MSDF_PADDING * 2;
        const double bitmapGlyphScale = target / FONT_METRICS_UNIT * 0.5;
        const double projectionScale = double(msdfSize - MSDF_PADDING * 2) / double(msdfSize);


        const auto offset_vx = MSDF_PADDING + offsetx * bitmapGlyphScale / projectionScale;
        const auto offset_vy = MSDF_PADDING + offsety * bitmapGlyphScale / projectionScale;

        //const auto offset = msdfgen::Vector2{
        //    MSDF_PADDING + double(offsetx) * bitmapGlyphScale / projectionScale,
        //    MSDF_PADDING + double(offsety) * bitmapGlyphScale / projectionScale
        //};

        offx = int32_t(double(offx) * projectionScale);
        offy = int32_t(double(offy) * projectionScale);
        //const auto timepoint1 = std::chrono::high_resolution_clock::now();
        msdfcc_shape_normalize(sink.msdfcc_shape);
        msdfcc_edge_coloring_simple(sink.msdfcc_shape, 3.0, 0);
        auto transformation = msdfcc_sdf_transformation_init({ { (msdfcc_float_t)projectionScale, (msdfcc_float_t)projectionScale }, { (msdfcc_float_t)offset_vx, (msdfcc_float_t)offset_vy } }, msdfcc_range_init1(4));
        msdfcc_size2d_t size = { (int32_t)msdfSize, (int32_t)msdfSize };
        auto config = msdfcc_error_correction_config_default();
        constexpr float sdfZeroValue = 0.5f;
        msdfcc_generate_mtsdf_overlap(bitmap, size, sink.msdfcc_shape, &transformation, &config);
        msdfcc_distance_sign_correction(
            bitmap, size, sink.msdfcc_shape,
            &transformation.projection, sdfZeroValue, msdfcc_fill_nonzero
        );
        config.distance_check_mode = msdfcc_ec_distance_check_do_not;
        msdfcc_msdf_error_correction_overlap(
            bitmap, size, sink.msdfcc_shape, &transformation, &config
        );
        //const auto timepoint2 = std::chrono::high_resolution_clock::now();
        //const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timepoint2 - timepoint1);
        //printf("msdfcc: %fms ", duration.count() / 1000.0);
        {
            //msdfgen::Bitmap<float, 4> msdf(msdfSize, msdfSize);
            //const auto timepoint3 = std::chrono::high_resolution_clock::now();
            //sink.shape.normalize();
            //msdfgen::edgeColoringSimple(sink.shape, 3.0);
            //msdfgen::SDFTransformation t(msdfgen::Projection({ projectionScale, projectionScale }, { offset_vx, offset_vy }), msdfgen::Range(4.0));
            //msdfgen::generateMTSDF(msdf, sink.shape, t);
            //msdfgen::distanceSignCorrection(msdf, sink.shape, t, sdfZeroValue, msdfgen::FILL_NONZERO);
            //msdfgen::msdfErrorCorrection(msdf, sink.shape, t);
            //const auto timepoint4 = std::chrono::high_resolution_clock::now();
            //const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timepoint4 - timepoint3);
            //printf("msdfgen: %fms\n", duration.count() / 1000.0);
            //const auto f1 = reinterpret_cast<float*>(bitmap);
            //const auto f2 = msdf(0, 0);
            //const uint32_t count = uint32_t(msdfSize) * uint32_t(msdfSize) * 4;
            //for (int i = 0; i != count; ++i) {
            //    if (f1[i] == f2[i]) {
            //    }
            //    else {
            //        printf("%d [%f] - [%f]\n", int(i), f1[i], f2[i]);
            //    }
            //}
            //const auto floats = f1;
            //impl::msdf_normalization_rgba(reinterpret_cast<uint8_t*>(buffer), floats, msdfSize);
            //impl::msdf_safe_zone_rgba(reinterpret_cast<uint8_t*>(buffer), msdfSize);
        }

        const auto floats = reinterpret_cast<float*>(bitmap);
        impl::msdf_normalization_rgba(reinterpret_cast<uint8_t*>(buffer), floats, msdfSize);
        impl::msdf_safe_zone_rgba(reinterpret_cast<uint8_t*>(buffer), msdfSize);
        charis_free(bitmap);
    }
    else {
        std::memset(buffer, 0, msdfSize * msdfSize * 4);
    }

    {
        Box2D box;
        box.x = point.x;
        box.y = point.y;
        box.width = msdfSize;
        box.height = msdfSize;

        GlyphTaskId task = {};
        task.msdf.type = GLYPH_TASK_TYPE_RGB_MSDF;
        task.msdf.bid = id;
        task.msdf.side = uint8_t(msdfSize);
        task.msdf.offsetX = offx;
        task.msdf.offsetY = offy;
        api->Upload(handle, task.id, box, buffer, msdfSize * 4);
    }
}

bool CharIS::CISBipedRenderer::draw_glyph_msdf(const biped_block_info_t& block, fp26dot6_t size, fppoint_t point, const TextEffect& effect, void* context) noexcept
{
    const auto bv = reinterpret_cast<BipedValue*>(block.value);
    if (bv->state == BIPED_STATE_READY) {

        if (!(block.real_size.w && block.real_size.h))
            return true;

        DrawTextureParam param = {};
        param.handle = m_hTextureRgb;
        param.type = TEXT_RENDER_TYPE_MSDF;
        param.size = size;

        param.dst.point.x = point.x + (mul(fp26dot6_t(bv->offsetX), size) >> 6);
        param.dst.point.y = point.y + (mul(fp26dot6_t(bv->offsetY), size) >> 6);
        param.dst.size.width = size;
        param.dst.size.height = size;

        param.src.x = block.position.x;
        param.src.y = block.position.y;
        param.src.width = block.real_size.w;
        param.src.height = block.real_size.h;

        m_pGraphicsApi->DrawAltas(param, effect, context);
        return true;
    }
    return false;
}

GlyphCache CharIS::CISBipedRenderer::create_cache_msdf(IISFontFace* face, char32_t ch) noexcept
{
    GlyphCache cache = {};

    const auto fontData = face->GetFontData();
    m_sKeyValue.key.fid = m_pParent->FamilyToId(fontData->family);
    m_sKeyValue.key.weight = fontData->weight;
    m_sKeyValue.key.stretch = fontData->stretch;
    m_sKeyValue.key.style = fontData->style;

    m_sKeyValue.key.ch = ch;
    m_sKeyValue.key.size = 0;

    biped_block_info_t block;
    auto result = biped_cache_lock_key(m_ctxBipedRgb, reinterpret_cast<uint32_t*>(&m_sKeyValue.key), &block);
    if (biped_is_success(result)) {
        const auto bv = reinterpret_cast<BipedValue*>(block.value);
        cache.biped.type = bv->type;
        cache.biped.bid = block.id;
    }
    else {
        IISFontFaceGlyph* glyph = nullptr;
        face->Lock(ch, &glyph);
        GlyphMetrics gm;
        if (glyph) {
            glyph->GetGlyphMetrics(&gm);
        }
        if (glyph) {
            face->Unlock(glyph);
            glyph = nullptr;
        }
        cache = this->add_biped_task_msdf(face, StandAloneComplex(&gm, face->GetFontMetrics(), face->GetFontData()));
    }
    return cache;
}

GlyphCache CharIS::CISBipedRenderer::add_biped_task_msdf(IISFontFace* face, uint16_t msdf) noexcept
{
    char32_t ch = m_sKeyValue.key.ch;

    biped_unit_t pixelSizeSide = std::max<uint16_t>(msdf, MSDF_SIZE_M);

    const biped_size2d_t pixelSize = { pixelSizeSide, pixelSizeSide };
    m_sKeyValue.value.type =
        msdf == MSDF_SIZE_L
        ? GLYPH_CACHE_TYPE_RGB_MSDF_L
        : GLYPH_CACHE_TYPE_RGB_MSDF_M
        ;
    m_sKeyValue.value.state = msdf ? BIPED_STATE_PENDING : BIPED_STATE_READY;
    biped_block_info_t block;
    auto result = biped_cache_lock_key_value(m_ctxBipedRgb, pixelSize, reinterpret_cast<uint32_t*>(&m_sKeyValue), &block);
    if (!biped_is_success(result)) {
        CHARIS_LOGGER_ERR("add_biped_task_msdf lock failed, required");
        return {};
    }

    if (!msdf) {
        GlyphCache cache = {};
        cache.biped.type = GLYPH_CACHE_TYPE_RGB_MSDF_M;
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
        const auto buffer = charis_malloc(size_t(msdf) * size_t(msdf) * 4);
        if (buffer) {
            charis_create_msdf(graphics, position, real_size, handle, face_ptr, ch, id, msdf, buffer);
            std::free(buffer);
        }
    });

    return {};
}
