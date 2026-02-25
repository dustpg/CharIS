#include "is_polygon_renderer.h"

using namespace CharIS;

CharIS::CISPolygonRenderer::CISPolygonRenderer(CISTextRenderer* parent) noexcept
{
}


CharIS::CISPolygonRenderer::~CISPolygonRenderer() noexcept
{
}


CODE CISPolygonRenderer::Init(uint32_t limit) noexcept 
{

    return CODE_OK;
}




void CISPolygonRenderer::TaskDone(uint64_t id) noexcept
{

}

void CISPolygonRenderer::ClearCache(GlyphCache list[], size_t len) noexcept
{

}

bool CISPolygonRenderer::DrawCache(GlyphCache cache, const TextEffect& effect,
    void* context, fp26dot6_t size, fppoint_t point) noexcept 
{

}

GlyphCache CISPolygonRenderer::CreateCache(IISFontFace* face,
    char32_t ch, fp26dot6_t size) noexcept {

    return create_cache_polygon(face, ch, size);
}

GlyphCache CharIS::CISPolygonRenderer::create_cache_polygon(IISFontFace* face, char32_t ch, fp26dot6_t size) noexcept
{
    return GlyphCache();
}
