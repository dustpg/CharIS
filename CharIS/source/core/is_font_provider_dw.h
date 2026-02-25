#pragma once
#include <CharIS/include/is_font_provider.h>
#include "is_helper_p.h"

struct IDWriteFactory;
struct IDWriteFontCollection;

namespace CharIS { namespace DirectWrite {

    class CISFontProvider : public impl::ref_count_class<CISFontProvider, IISFontProvider> {
    public:

        CISFontProvider() noexcept;

        ~CISFontProvider() noexcept;

        CODE DefaultFont(char16_t name[], uint32_t buflen) noexcept override;

        CODE FindFont(
            const char16_t* name,
            FONT_WEIGHT,
            FONT_STRETCH,
            FONT_STYLE,
            uint32_t pathlen,
            char16_t path[],
            uint32_t& face_index
        ) noexcept override;

        CODE Init() noexcept;

    protected:

        IDWriteFactory*                 m_pDWriteFactory = nullptr;

        IDWriteFontCollection*          m_pFontCollection = nullptr;

    };

}}
