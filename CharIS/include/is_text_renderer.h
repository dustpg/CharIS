#pragma once
#include "is_base.h"

namespace CharIS {

    struct IISFontEngine;
    struct IISInlineObject;
    struct IISTextDocument;
    struct IISGraphics;

    enum TEXT_MISS_BEHAVIOR : uint8_t {
        TEXT_MISS_BEHAVIOR_WAIT = 0,
        TEXT_MISS_BEHAVIOR_SKIP_CHARACTER,
        TEXT_MISS_BEHAVIOR_SKIP_LINE,       // default
        TEXT_MISS_BEHAVIOR_SKIP_PARAGRAPH,
    };

    enum TEXT_RENDERER_HINT : uint8_t {
        // 0-32 GRAY/SUBPIXEL 32-64 [GRAY/MSDF] 64-1024[MSDF] 1024+[POLYGON]
        TEXT_RENDERER_HINT_AUTO = 0,
        TEXT_RENDERER_HINT_GRAY, 
        TEXT_RENDERER_HINT_SUBPIXEL,
        TEXT_RENDERER_HINT_MSDF, 
        TEXT_RENDERER_HINT_POLYGON,
    };


    struct alignas(uint32_t) TextRenderParam {

        TEXT_MISS_BEHAVIOR missBehavior = TEXT_MISS_BEHAVIOR_SKIP_LINE;

        TEXT_RENDERER_HINT rendererHint = TEXT_RENDERER_HINT_AUTO;

    };


    // private class
    struct IS_INTERFACE IISTextRenderer : IISBase {
        // call this per-frame
        virtual void Update(const uint64_t* tasks = nullptr, size_t length = 0) noexcept = 0;

    };

    struct CreateTextRendererParam {
        uint32_t    altasWidthHeight; // 512~2048 Biped Cache [0 to disable]
        uint32_t    polygonVertexMax; //  COST+LRU Cache   [0 to disable]

        bool        subpixelSupported;
        bool        msdfSupported;
        uint8_t     taskThreadCount; // 0: work in render thread 1+: work thread count

    };

}


extern "C" CharIS::CODE CharisCreateTextRenderer(
    CharIS::IISTextRenderer** renderer,
    CharIS::IISFontEngine* engine,
    CharIS::IISGraphics* graphics,
    const CharIS::CreateTextRendererParam* param
) noexcept;


static inline CharIS::CODE CharisCreateTextRenderer(
    CharIS::IISTextRenderer** renderer,
    CharIS::IISFontEngine* engine,
    CharIS::IISGraphics* graphics,
    const CharIS::CreateTextRendererParam& param
) noexcept {
    return CharisCreateTextRenderer(renderer, engine, graphics, &param);
}
