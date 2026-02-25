#pragma once
#include <CharIS/include/is_base.h>
#include <CharIS/include/is_font.h>

namespace CharIS {

    enum TEXTURE_CHANNEL : uint32_t {
        /*
        Ideal state: one RGBA texture.
            R channel stores grayscale information.
            GBA channels store subpixel/msdf information.
        */

        // gray
        TEXTURE_CHANNEL_R8 = 0,
        // subpixel/msdf
        TEXTURE_CHANNEL_R8G8B8,

    };

    struct CreateTextureParam {

        uint32_t        side;

        TEXTURE_CHANNEL channel;
    };

    enum TEXT_RENDER_TYPE : uint32_t {
        TEXT_RENDER_TYPE_GRAY = 0, //  one by one pixel
        TEXT_RENDER_TYPE_SUBPIXEL,
        //TEXT_RENDER_TYPE_SDF, 
        TEXT_RENDER_TYPE_MSDF,
        TEXT_RENDER_TYPE_POLYGON,
    };

    struct Box2Dfp { fppoint_t point; fpsize_t size; };

    struct Box2D { uint32_t x, y, width, height; };

    struct DrawTextureParam {
        // texture handle
        uint64_t            handle;

        // type GRAY/SUBPIXEL/MSDF
        TEXT_RENDER_TYPE    type;

        fp26dot6_t          size;

        // DEST/SRC

        Box2Dfp             dst;

        Box2D               src;

    };

    /// <summary>
    /// Graphics interface that must be implemented by the user
    /// </summary>
    struct IS_INTERFACE IISGraphics {

        virtual void DisposeTexture(uint64_t handle) noexcept = 0;

        virtual CODE CreateAltasTexture(CreateTextureParam param, uint64_t& handle) noexcept = 0;

        virtual void DrawAltas(const DrawTextureParam& draw, const TextEffect& effect, void* context) noexcept = 0;

        virtual void Upload(uint64_t tex, uint64_t task, Box2D box, const void* data, uint32_t pitch) noexcept = 0;

        virtual void FillRect(uint32_t background, void* context, fppoint_t point, fpsize_t size) noexcept = 0;
    };

}