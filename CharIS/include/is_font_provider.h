#pragma once
#include "is_base.h"
#include "is_font.h"

namespace CharIS {

    struct IS_INTERFACE IISFontProvider : IISBase {

        //virtual uint16_t FindId(char16_t* name, int weight,)

        virtual CODE DefaultFont(char16_t name[], uint32_t buflen) noexcept = 0;

        virtual CODE FindFont(
            const char16_t* name, 
            FONT_WEIGHT, 
            FONT_STRETCH, 
            FONT_STYLE,
            uint32_t pathlen, 
            char16_t path[], 
            uint32_t& face_index
        ) noexcept = 0;
    };
}



extern "C" CharIS::CODE CharisCreateFontProvider(CharIS::IISFontProvider** database, int flags = 0) noexcept;
