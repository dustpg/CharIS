#pragma once
#include "is_base.h"
#include "is_font.h"
#include "is_text_renderer.h"

namespace CharIS {

    /// <summary>
    /// 
    /// </summary>
    struct TextViewport {

        fp26dot6_t  x, y, w, h;

    };

    /// <summary>
    /// 
    /// </summary>
    struct HitTestCtx {

        fppoint_t       caret1;

        fppoint_t       caret2;

        uint32_t        postion;
    };


    /// <summary>
    /// 
    /// </summary>
    struct TextDraw {

        uint32_t        count;

        uint32_t        drawn;
    };

    // VerticalAlign
    enum VERTICAL_ALIGN : uint16_t {
        // baseline
        VERTICAL_ALIGN_BASELINE = 0,
        // ascender
        VERTICAL_ALIGN_ASCENDER,
        // middle
        VERTICAL_ALIGN_MIDDLE,
        // descender
        VERTICAL_ALIGN_DESCENDER,
    };

    // TextAlign
    enum TEXT_ALIGN : uint16_t {
        // leading/begin
        TEXT_ALIGN_LEADING = 0,
        // center
        TEXT_ALIGN_CENTER,
        // trailing/end
        TEXT_ALIGN_TRAILING,
        // justified
        TEXT_ALIGN_JUSTIFIED,
    };

    // wrap mode
    enum WRAP_MODE : uint16_t {
        // no wrap
        WRAP_MODE_NO_WRAP = 0,
        // space(include tab) only
        WRAP_MODE_SPACE_ONLY,
        // wrap cjk anwhere, other up to space
        WRAP_MODE_SPACE_OR_CJK,
        // anwhere
        WRAP_MODE_ANY_WHERE,
        // layout-dependent custom implementation
        WRAP_MODE_ANY_AUTO,
    };

    /// <summary>
    /// 
    /// </summary>
    enum TEXT_DIRECTION : uint32_t {

        TEXT_DIRECTION_LEFT_TO_RIGHT = 0,

        TEXT_DIRECTION_TOP_TO_BOTTOM,

        TEXT_DIRECTION_RIGHT_TO_LEFT,

        TEXT_DIRECTION_BOTTOM_TO_TOP,
    };


    /// <summary>
    /// Coordinate System
    /// </summary>
    enum TEXT_COORDINATE_SYSTEM : uint32_t {

        TEXT_COORDINATE_SYSTEM_DOCUMENT,

        TEXT_COORDINATE_SYSTEM_VIEWPORT,

    };


    /// <summary>
    /// 
    /// </summary>
    struct TextViewportRange {

        fp26dot6_t                      left;

        fp26dot6_t                      top;

        fp26dot6_t                      right;

        fp26dot6_t                      bottom;

        TEXT_COORDINATE_SYSTEM       coordinate;

    };

    /// <summary>
    /// 
    /// </summary>
    /// <seealso cref="IISDispose" />
    struct IS_INTERFACE IISTextLayout : IISBase {

        virtual TextDraw Draw(void* context, TextRenderParam param, const TextViewportRange* range = nullptr) noexcept = 0;

        virtual CODE SetFontSize(Range range, fp26dot6_t) noexcept = 0;

        virtual CODE SetFontFace(Range range, const char16_t*, FONT_WEIGHT = FONT_WEIGHT_NORMAL, FONT_STYLE = FONT_STYLE_NORMAL, FONT_STRETCH = FONT_STRETCH_NORMAL) noexcept = 0;

        virtual CODE SetSubEffect(Range range, uint32_t offset, uint32_t value) noexcept = 0;

        virtual CODE SetEffectFlag(Range range, uint32_t bitIndex, bool on) noexcept = 0;

        virtual CODE SetDirection(TEXT_DIRECTION reading, TEXT_DIRECTION paragraph) noexcept = 0;

        virtual CODE SetViewpoint(fppoint_t pos, TEXT_COORDINATE_SYSTEM = TEXT_COORDINATE_SYSTEM_VIEWPORT) noexcept = 0;

        virtual CODE SetCanvasSize(fpsize_t size, TEXT_COORDINATE_SYSTEM = TEXT_COORDINATE_SYSTEM_VIEWPORT) noexcept = 0;

        virtual CODE GetLayoutSize(fpsize_t& size, TEXT_COORDINATE_SYSTEM = TEXT_COORDINATE_SYSTEM_VIEWPORT) noexcept = 0;

        virtual CODE HitTest(fppoint_t point, HitTestCtx&, TEXT_COORDINATE_SYSTEM = TEXT_COORDINATE_SYSTEM_VIEWPORT) noexcept = 0;

        virtual CODE HitTest(uint32_t position, HitTestCtx&, TEXT_COORDINATE_SYSTEM = TEXT_COORDINATE_SYSTEM_VIEWPORT) noexcept = 0;

    };

}
