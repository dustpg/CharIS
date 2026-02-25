#pragma once
#include "is_base.h"
#include "is_font_face.h"

namespace CharIS {


    /// <summary>
    /// 
    /// </summary>
    struct IS_INTERFACE IISFontEngine : IISBase {

        virtual CODE MatchFontFace(
            IISFontFace**, const char16_t* name,
            FONT_WEIGHT = FONT_WEIGHT_NORMAL,
            FONT_STYLE = FONT_STYLE_NORMAL,
            FONT_STRETCH = FONT_STRETCH_NORMAL
        ) = 0;

        virtual CODE SetGlobalFallbackList(const char16_t* list) noexcept = 0;

        //virtual CODE SetSpecificFallbackList(const char16_t* name, const char16_t* list) noexcept = 0;

    };

}

extern "C" CharIS::CODE CharisCreateFontEngine(
    CharIS::IISFontEngine** manager, 
    CharIS::IISFontProvider* database,
    int rev = 0
) noexcept;
