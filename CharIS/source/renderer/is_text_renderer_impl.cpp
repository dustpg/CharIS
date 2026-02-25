#include "is_text_renderer_impl.h"
#include <CharIS/include/is_base.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>

using namespace CharIS;



/// <summary>
/// Fills a rectangle with the specified background color.
/// </summary>
/// <param name="background">The background color.</param>
/// <param name="context">The rendering context.</param>
/// <param name="param">The render parameters.</param>
/// <param name="point">The top-left point of the rectangle.</param>
/// <param name="size">The size of the rectangle.</param>
void CISTextRenderer::FillRect(uint32_t background, void* context, TextRenderParam param, fppoint_t point, fpsize_t size) noexcept
{
    m_pGraphicsApi->FillRect(background, context, point, size);
}

/// <summary>
/// Draws a single character at the specified position.
/// </summary>
/// <param name="face">The font face to use.</param>
/// <param name="effect">The text effect (colors, flags, etc.).</param>
/// <param name="context">The rendering context.</param>
/// <param name="param">The render parameters.</param>
/// <param name="ch">The character to draw (UTF-32).</param>
/// <param name="size">The font size in 26.6 fixed point.</param>
/// <param name="point">The position to draw at.</param>
/// <returns>True if the character was drawn successfully, false otherwise.</returns>
bool CISTextRenderer::DrawGlyph(IISFontFace* face, const TextEffect& effect, GlyphCache& cache, void* context, TextRenderParam param, char32_t ch, fp26dot6_t size, fppoint_t point) noexcept
{

    const auto l = [=]() noexcept {
        GlyphCache c = {};

        if (!c.biped.type) {
            c = m_oPolygonRenderer.CreateCache(face, ch, size);
        }
        if (c.biped.type) {
            return m_oPolygonRenderer.DrawCache(c, effect, context, size, point);
        }
        return false;
    };

    l();

    auto c = cache;

    if (!c.biped.type) {
        c = cache = m_oBipedRenderer.CreateCache(face, ch, size);
    }
    if (c.biped.type) {
        return m_oBipedRenderer.DrawCache(c, effect, context, size, point);
    }

    return false;
}

/// <summary>
/// Draws an inline object at the specified position.
/// </summary>
/// <param name="obj">The inline object to draw.</param>
/// <param name="point">The position to draw at.</param>
/// <param name="context">The rendering context.</param>
/// <param name="param">The render parameters.</param>
/// <returns>True if the object was drawn successfully, false otherwise.</returns>
bool CISTextRenderer::DrawInlineObject(IISInlineObject* obj, fppoint_t point, void* context, TextRenderParam param) noexcept
{
    // TODO: Implement inline object drawing
    (void)obj;
    (void)point;
    (void)context;
    (void)param;
    return false;
}

/// <summary>
/// Sets a rendering attribute.
/// </summary>
/// <param name="attr">The attribute name.</param>
/// <param name="value">The attribute value.</param>
/// <returns>True if the attribute was set successfully, false otherwise.</returns>
bool CISTextRenderer::SetAttribute(const char* attr, int32_t value) noexcept
{
    // TODO: Implement attribute setting
    (void)attr;
    (void)value;
    return false;
}

CharIS::CISTextRenderer::CISTextRenderer(IISFontEngine* engine, IISGraphics* graphics) noexcept
    : m_pFontEngine(engine)
    , m_pGraphicsApi(graphics)
    , m_oBipedRenderer(this)
    , m_oPolygonRenderer(this)
{
    assert(engine && graphics);
    engine->AddRefCnt();
    charis_family_id_ctx_init(&m_ctxFamilyId);
}

CharIS::CISTextRenderer::~CISTextRenderer() noexcept
{
    // TODO: Force clear & Shut down
    m_oThreadPool.Shutdown();

    charis_family_id_ctx_uninit(&m_ctxFamilyId);
    m_pGraphicsApi = nullptr;
    CharIS::SafeDispose(m_pFontEngine);
}

void CharIS::CISTextRenderer::Update(const uint64_t* ids, size_t length) noexcept
{
    for (size_t i = 0; i != length; ++i) {
        const auto id = ids[i];
        m_oBipedRenderer.TaskDone(id);
        m_oPolygonRenderer.TaskDone(id);
    }
}

void CharIS::CISTextRenderer::ClearCache(GlyphCache list[], size_t len) noexcept 
{
    m_oBipedRenderer.ClearCache(list, len);
    m_oPolygonRenderer.ClearCache(list, len);
}

CODE CharIS::CISTextRenderer::Init(const CreateTextRendererParam& param) noexcept
{
    CODE code = CODE_OK;

    if (!m_oThreadPool.Init(param.taskThreadCount)) {

        code = CODE_OUTOFMEMORY;

    }

    // Biped
    if (const auto side = param.altasWidthHeight) {

        code = m_oBipedRenderer.Init(side, param.msdfSupported || param.subpixelSupported, true);

    }

    // Polygon
    if (const auto cost = param.polygonVertexMax) {

        code = m_oPolygonRenderer.Init(cost);

    }

    return code;
}




/// <summary>
/// Creates a text renderer(also as manager) instance.
/// </summary>
/// <param name="renderer">Output parameter for the created renderer.</param>
/// <param name="engine">The font engine </param>
/// <param name="graphics">The graphics api.</param>
/// <returns>CODE_OK on success, error code otherwise.</returns>
extern "C" CharIS::CODE CharisCreateTextRenderer(
    CharIS::IISTextRenderer** renderer,
    CharIS::IISFontEngine* engine,
    CharIS::IISGraphics* graphics,
    const CharIS::CreateTextRendererParam* param
) noexcept
{
    assert(renderer && engine && param && graphics);
    if (!renderer || !engine || !graphics || !param)
        return CharIS::CODE_POINTER;

    CharIS::CODE code = CharIS::CODE_OK;
    CharIS::CISTextRenderer* obj = nullptr;

    // Create renderer instance
    obj = new (std::nothrow) CharIS::CISTextRenderer{ engine, graphics };
    if (!obj) {
        code = CharIS::CODE_OUTOFMEMORY;
    }

    // Init
    if (CharIS::Success(code)) {
        code = obj->Init(*param);
    }

    // Output the created renderer
    if (CharIS::Success(code)) {
        *renderer = CharIS::Take(obj);
    }

    CharIS::SafeDispose(obj);
    return code;
}
