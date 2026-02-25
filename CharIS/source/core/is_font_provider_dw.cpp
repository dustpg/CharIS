#include "is_font_provider_dw.h"
#include "is_common.h"
#include <cstdio>

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>
#include <dwrite_1.h>
#include <cassert>

#define INITGUID
#include <guiddef.h>
DEFINE_GUID(IID_IDWriteLocalFontFileLoader, 0xb2d9f3ec, 0xc9fe, 0x4a11, 0xa2, 0xec, 0xd8, 0x62, 0x08, 0xf7, 0xc0, 0xa2);
DEFINE_GUID(IID_IDWriteFactory, 0xb859ee5a, 0xd838, 0x4b5b, 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48);
using namespace CharIS;


/// <summary>
/// Finalizes an instance of the <see cref="CISFontProvider"/> class.
/// </summary>
/// <returns></returns>
DirectWrite::CISFontProvider::~CISFontProvider() noexcept
{
    CharIS::SafeRelease(m_pFontCollection);
    CharIS::SafeRelease(m_pDWriteFactory);
}

/// <summary>
/// Initializes this instance.
/// </summary>
/// <returns></returns>
auto DirectWrite::CISFontProvider::Init() noexcept -> CODE
{
    assert(m_pFontCollection == nullptr);
    assert(m_pDWriteFactory == nullptr);

    HRESULT hr = ::DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        IID_IDWriteFactory,
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );

    // FONT COLLECTION
    if (SUCCEEDED(hr)) {
        hr = m_pDWriteFactory->GetSystemFontCollection(&m_pFontCollection, FALSE);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("GetSystemFontCollection failed: %08X\n", hr);
        }
    }

    return CharIS::HResult(hr);
}


/// <summary>
/// Initializes a new instance of the <see cref="CISFontProvider"/> class.
/// </summary>
DirectWrite::CISFontProvider::CISFontProvider() noexcept
{

}

/// <summary>
/// Finds the font.
/// </summary>
/// <param name="name">The name.</param>
/// <param name="weight">The weight.</param>
/// <param name="stretch">The stretch.</param>
/// <param name="style">The style.</param>
/// <param name="pathlen">The pathlen.</param>
/// <param name="path">The path.</param>
/// <param name="face_index">Index of the face.</param>
/// <returns></returns>
auto DirectWrite::CISFontProvider::FindFont(
    const char16_t* name, 
    FONT_WEIGHT weight,
    FONT_STRETCH stretch, 
    FONT_STYLE style,
    uint32_t pathlen, 
    char16_t path[], 
    uint32_t& face_index) noexcept -> CODE
{
    HRESULT hr = S_OK;
    IDWriteFontFamily* pFontFamily = nullptr;
    IDWriteFont* pFont = nullptr;
    IDWriteFontFace* pFontFace = nullptr;
    IDWriteFontFile* pFontFile = nullptr;
    IDWriteFontFileLoader* pLoader = nullptr;
    IDWriteLocalFontFileLoader* pLocal = nullptr;
    const void* pRefKey = nullptr;
    UINT32 refKeySize = 0;

    UINT32 index = 0;
    BOOL exists = false;
    // FIND
    if (SUCCEEDED(hr)) {
        hr = m_pFontCollection->FindFamilyName((const wchar_t*)name, &index, &exists);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("FindFamilyName failed: %08X\n", hr);
        }
    }
    // FAMILY
    if (SUCCEEDED(hr)) {
        if (exists) {
            hr = m_pFontCollection->GetFontFamily(index, &pFontFamily);
            if (FAILED(hr)) {
                CHARIS_LOGGER_ERR("GetFontFamily failed: %08X\n", hr);
            }
        }
        else {
            CHARIS_LOGGER_ERR("%s: FONT NOT FOUND(%p)\n", __FUNCTION__, name);
            hr = E_INVALIDARG;
        }
    }
    // MATCH
    if (SUCCEEDED(hr)) {
        const auto dws = style == FONT_STYLE_NORMAL ? DWRITE_FONT_STYLE_NORMAL : DWRITE_FONT_STYLE_ITALIC;
        hr = pFontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT(weight), DWRITE_FONT_STRETCH(stretch), dws, &pFont);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("GetFirstMatchingFont failed: %08X\n", hr);
        }
    }
    // FACE
    if (SUCCEEDED(hr)) {
        pFont->CreateFontFace(&pFontFace);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("CreateFontFace failed: %08X\n", hr);
        }
    }
    // FILE
    if (SUCCEEDED(hr)) {
        face_index = pFontFace->GetIndex();
        index = 1;
        hr = pFontFace->GetFiles(&index, &pFontFile);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("GetFiles failed: %08X\n", hr);
        }
    }
    // TEST
#ifndef NDEBUG
    if (SUCCEEDED(hr)) {
        DWRITE_FONT_METRICS fm;
        pFontFace->GetMetrics(&fm);

        UINT32 ch = U'g';
        UINT16 id = 0;
        pFontFace->GetGlyphIndices(&ch, 1, &id);

        DWRITE_GLYPH_METRICS gm = {};
        pFontFace->GetDesignGlyphMetrics(&id, 1, &gm);

        int bk = 9;
    }
#endif
    // KEY
    if (SUCCEEDED(hr)) {
        hr = pFontFile->GetReferenceKey(&pRefKey, &refKeySize);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("GetReferenceKey failed: %08X\n", hr);
        }
    }
    // LOADER
    if (SUCCEEDED(hr)) {
        hr = pFontFile->GetLoader(&pLoader);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("GetLoader failed: %08X\n", hr);
        }
    }
    // LOCAL FILE
    if (SUCCEEDED(hr)) {
        hr = pLoader->QueryInterface(IID_IDWriteLocalFontFileLoader, (void**)&pLocal);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("QueryInterface failed: %08X\n", hr);
        }
    }
    // GET PATH
    if (SUCCEEDED(hr)) {
        static_assert(sizeof(WCHAR) == sizeof(char16_t), "UTF16");
        hr = pLocal->GetFilePathFromKey(pRefKey, refKeySize, (WCHAR*)path, pathlen);
        if (FAILED(hr)) {
            CHARIS_LOGGER_ERR("GetFilePathFromKey failed: %08X\n", hr);
        }
    }
    CharIS::SafeRelease(pLocal);
    CharIS::SafeRelease(pLoader);
    CharIS::SafeRelease(pFontFile);
    CharIS::SafeRelease(pFont);
    CharIS::SafeRelease(pFontFace);
    CharIS::SafeRelease(pFontFamily);

    return CharIS::HResult(hr);
}

/// <summary>
/// Defaults the font.
/// </summary>
/// <param name="name">The name.</param>
/// <param name="buflen">The buflen.</param>
/// <returns></returns>
auto DirectWrite::CISFontProvider::DefaultFont(char16_t name[], uint32_t buflen) noexcept -> CODE
{
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        constexpr uint32_t len = sizeof(ncm.lfMessageFont.lfFaceName) / sizeof(ncm.lfMessageFont.lfFaceName[0]);

        if (buflen < len)
            return CODE_SMALLBUFFER;

        std::memcpy(name, ncm.lfMessageFont.lfFaceName, sizeof(ncm.lfMessageFont.lfFaceName));
        return CODE_OK;
    }

    return CODE_ABORT;
}


extern "C" CharIS::CODE CharisCreateFontProvider(IISFontProvider** database, int flags) noexcept{
    assert(database);
    if (const auto obj = new (std::nothrow) DirectWrite::CISFontProvider) {
        const auto code = obj->Init();
        if (CharIS::Success(code)) {
            *database = obj;
            return code;
        }
        obj->Dispose();
        return code;
    }
    return CODE_OUTOFMEMORY;
}
