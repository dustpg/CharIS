#pragma once
#include <CharIS/include/is_font_engine.h>
#include <CharIS/include/is_font_provider.h>
#include "is_helper_p.h"
#include "../utils/is_thread_impl.h"

#include <ft2build.h>
#include FT_FREETYPE_H 

// TODO: REMOVE STL
#include <unordered_map>
#include <vector>


namespace CharIS { namespace FreeType {

    enum { FT_MAX_THREAD_INSTANCE = 4, FT_CACHE_QUEUE_LEN = 2 };

    enum OBJECT_POS : uint32_t {
        OBJECT_POS_MAP = 0,
        OBJECT_POS_CACHE,
    };

    class CISFontEngine;

    struct FontEx { size_t hash; Font font; };

    struct FontFamilyEx { size_t hash; FontFamily family; };

    struct FONT_HASH { static size_t impl(const FontEx& ts) noexcept; size_t operator()(const FontEx& ts) const noexcept { return ts.hash; } };

    struct FONT_EQUL { bool operator()(const FontEx& l, const FontEx& r) const noexcept; };


    class CISFaceGlyph final : public IISFontFaceGlyph {
    public:

        uintptr_t   GetHandle() const noexcept override;

        void        GetGlyphMetrics(GlyphMetrics*) noexcept override;

        int32_t     GetKerning(char32_t left, char32_t right) noexcept override;

        CODE        GetOutline(IISGeometrySink*, double size) noexcept override;

        CODE        RenderToBitmap(fp26dot6_t size, BitmapInfo*, BITMA_PRENDER_MODE = BITMA_PRENDER_MODE_NORMAL) noexcept override;

    public:

        CODE        Init(const char16_t* path, uint32_t index, CISFontEngine*) noexcept;

        auto        GetFace() const noexcept { return m_hFontFace; }

        uint32_t    GetIndex(char32_t) noexcept;

        CODE        LoadGlyph(uint32_t, int flag) noexcept;

        //void        Move(CISFaceGlyph&&) noexcept;

        //void        Done() noexcept;

        CISFaceGlyph() noexcept;

        ~CISFaceGlyph() noexcept;

    protected:

        FT_Face                 m_hFontFace = nullptr;

        FT_StreamRec            m_sStream = {};

    };


    class CISFaceCore {
    public:

        ~CISFaceCore() noexcept;

        CISFaceCore(const Font& ts) noexcept;

        CISFaceCore(CISFaceCore&&) noexcept = delete;

        CISFaceCore(const CISFaceCore&) noexcept = delete;

        CODE Init(CISFontEngine*) noexcept;
        // CODE_SMALLBUFFER FOR out of bitmap 
        CODE Borrow(CISFontEngine*, CISFaceGlyph**) noexcept;

        void Return(uint32_t index) noexcept;

        uint32_t Returnable(CISFaceGlyph*) const noexcept;

        //void Swap(CISFaceCore&) noexcept;

        //void Move(CISFaceCore&&) noexcept;

    public:

        //void Done() noexcept;

        bool Check() const noexcept { return m_sFontData.length != 0; }

        const Font* GetFontData() const noexcept { return &m_sFontData; }

        const FontMetrics* GetFontMetrics() const noexcept { return &m_sFontMetrics; }

        OBJECT_POS              position = OBJECT_POS_MAP;

    protected:

        uint32_t                m_bitmap = (1 << FT_MAX_THREAD_INSTANCE) - 1;

        Font                    m_sFontData = {};

        FontMetrics             m_sFontMetrics = {};

        CISFaceGlyph            m_map[FT_MAX_THREAD_INSTANCE];

    };

    struct IndexResult { CODE code; uint32_t index; };

    class CISFontFace final : public impl::ref_count_move<CISFontFace, IISFontFace> {

    public:

        const Font*         GetFontData() const noexcept override;

        const FontMetrics*  GetFontMetrics() const noexcept override;

        CODE                Lock(char32_t ch, IISFontFaceGlyph**) noexcept override;

        void                Unlock(IISFontFaceGlyph* glyph) noexcept override;

    public:

        auto& RefCore() noexcept { return m_core; }

        CODE Init() noexcept;

        ~CISFontFace() noexcept;

        //CISFontFace(CISFontEngine& engine) noexcept;

        CISFontFace(CISFontEngine& engine, const Font& ts) noexcept;

        CISFontFace(const CISFontFace&) noexcept = delete;

        CODE LockBase(CISFaceGlyph**) noexcept;

        void UnlockBase(uint32_t) noexcept;

        bool Move() noexcept;

    protected:

        CISFontEngine&              m_refEngine;

        impl::mtx                   m_mtx = {};

        impl::cnd                   m_cnd = {};

        CISFaceCore                 m_core;

    };


    class CISFontEngine final : public impl::ref_count_class<CISFontEngine, IISFontEngine> {

        void discard() noexcept;

        void clear_global_fallback() noexcept;

        void clear_cache() noexcept;

    public:

        CODE MatchFontFace(IISFontFace**, const char16_t* name, FONT_WEIGHT, FONT_STYLE, FONT_STRETCH) noexcept override;

        CODE SetGlobalFallbackList(const char16_t* list) noexcept override;

        //CODE SetSpecificFallbackList(const char16_t* name, const char16_t* list) noexcept override;

    public:

        CISFontEngine() noexcept;

        ~CISFontEngine() noexcept;

    public:


        CODE Init(IISFontProvider*) noexcept;

        auto Database() noexcept { return m_pFontProvider; }

        auto Freetype() noexcept { return m_hLibrary; }

        void Created(const CISFontFace&) noexcept;

        void Disposed(const CISFontFace&) noexcept;

        bool TryMove(CISFontFace&) noexcept;

        auto FallbackLock(char32_t ch, CISFaceGlyph**) noexcept -> IndexResult;

        void FallbackUnlock(CISFaceGlyph*) noexcept;

    protected:

        CODE matchFontFaceEx(IISFontFace**, const char16_t* name, uint32_t len, FONT_WEIGHT, FONT_STYLE, FONT_STRETCH) noexcept;

        void advIndex() noexcept;
        
        FT_Library                      m_hLibrary = nullptr;

        IISFontProvider*                m_pFontProvider = nullptr;

        std::vector<CISFontFace*>       m_vGlobalFallbackList;

        std::unordered_map<FontEx, CISFontFace*, FONT_HASH, FONT_EQUL >
                                        m_map;

        CISFontFace*                    m_apCached[FT_CACHE_QUEUE_LEN] = {};

        uint32_t                        m_uCounter = 0;
        
        uint32_t                        m_uRandomIndex = 0;

    };

}}
