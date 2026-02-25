#pragma once
#include "is_base.h"
#include "is_font.h"
#include "is_geometry_sink.h"

namespace CharIS {

    //struct IDWriteFontFace;
    //typedef struct FT_FaceRec_*  FT_Face;
    struct IISFontProvider;


    enum { FONT_METRICS_UNIT = 4096 };

    /// <summary>
    /// 
    /// </summary>
    struct FontMetrics {

        //int16_t                 units;

        int16_t                 ascent;

        int16_t                 descent;

        int16_t                 underlinePosition;

        int16_t                 underlineThickness;

        int16_t                 strikethroughPosition;

        int16_t                 strikethroughThickness;

        int16_t                 weiget;

        bool                    kerningSupport;

    };

    /// <summary>
    /// dwrite style glyph metrics
    /// </summary>
    struct GlyphMetrics {
        /*
        Specifies the X offset from the glyph origin to the left edge of the black
        box. The glyph origin is the current horizontal writing position.
        A negative value means the black box extends to the left of the origin
        (often true for lowercase italic 'f').
        */
        int32_t             leftSideBearing;
        /*
        Specifies the X offset from the origin of the current glyph to the origin
        of the next glyph when writing horizontally.
        */
        int32_t             advanceWidth;
        /*
        Specifies the X offset from the right edge of the black box to the origin
        of the next glyph when writing horizontally. The value is negative when
        the right edge of the black box overhangs the layout box.
        */
        int32_t             rightSideBearing;
        /*
        Specifies the vertical offset from the vertical origin to the top of
        the black box. Thus, a positive value adds whitespace whereas a negative
        value means the glyph overhangs the top of the layout box.
        */
        int32_t             topSideBearing;
        /*
        Specifies the Y offset from the vertical origin of the current glyph
        to the vertical origin of the next glyph when writing vertically. Note
        that the term "origin" by itself denotes the horizontal origin. The
        vertical origin is different. Its Y coordinate is specified by
        verticalOriginY value, and its X coordinate is half the advanceWidth
        to the right of the horizontal origin.
        */
        int32_t             advanceHeight;
        /*
        Specifies the vertical distance from the bottom edge of the black box
        to the advance height. This is positive when the bottom edge of the black
        box is within the layout box, or negative when the bottom edge of black
        box overhangs the layout box.
        */
        int32_t             bottomSideBearing;
        /*
        Specifies the Y coordinate of a glyph's vertical origin, in the font's
        design coordinate system. The y coordinate of a glyph's vertical origin
        is the sum of the glyph's top side bearing and the top (that is, yMax)
        of the glyph's bounding box.
        */
        int32_t             verticalOriginY;



        uint16_t            points;

        uint16_t            contours;

    };

    enum BITMAP_PIXEL_FORAMT : uint32_t {
        BITMAP_PIXEL_FORAMT_R8 = 0,  // [R-PIXEL]
        BITMAP_PIXEL_FORAMT_RGB8, // [R-PIXEL G-PIXEL B-PIXEL]
        BITMAP_PIXEL_FORAMT_RGB8_PLANARLINE, // LINE0[R-PLANE G-PLANE B-PLANE] LINE1[...]
    };


    struct BitmapInfo {
        void*               data;
        uint32_t            width; 
        uint32_t            height;
        uint32_t            pitch;  // in byte = width * bytePepPixel + aligned
        BITMAP_PIXEL_FORAMT format;
        int32_t             offsetX;
        int32_t             offsetY;
    };

    enum BITMA_PRENDER_MODE : uint32_t {
        BITMA_PRENDER_MODE_NORMAL = 0,       // [8->Gray]
        BITMA_PRENDER_MODE_MONO,             // [1->MONO 8->Gray]
        BITMA_PRENDER_MODE_LCD_HORIZONTAL,   // [24->Subpixel]
        BITMA_PRENDER_MODE_LCD_VERTICAL,     // [24->Subpixel]
    };


    /// <summary>
    /// 
    /// </summary>
    struct IS_INTERFACE IISFontFaceGlyph {
        // using low-api directly
        // FreeType: FT_Face
        // DIrectWrite: IDWriteFontFace
        virtual uintptr_t   GetHandle() const noexcept = 0;

        virtual void        GetGlyphMetrics(GlyphMetrics*) noexcept = 0;

        virtual int32_t     GetKerning(char32_t left, char32_t right) noexcept = 0;

        virtual CODE        GetOutline(IISGeometrySink*, double size) noexcept = 0;

        virtual CODE        RenderToBitmap(fp26dot6_t size, BitmapInfo*, BITMA_PRENDER_MODE = BITMA_PRENDER_MODE_NORMAL) noexcept = 0;

    };

    /// <summary>
    /// 
    /// </summary>
    struct IS_INTERFACE IISFontFace : IISBase {

        virtual const Font*         GetFontData() const noexcept = 0;

        virtual const FontMetrics*  GetFontMetrics() const noexcept = 0;

        virtual CODE                Lock(char32_t, IISFontFaceGlyph**) noexcept = 0;

        virtual void                Unlock(IISFontFaceGlyph* ) noexcept = 0;

    };

}
