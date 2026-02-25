#pragma once

#include <cstdint>
#include <cassert>
#include <utility>

#if defined(__clang__)
// [[clang::novtable]]
#define CHARIS_NO_VTABLE __declspec(novtable)
#elif defined(_MSC_VER)
#define CHARIS_NO_VTABLE __declspec(novtable)
#else
#define CHARIS_NO_VTABLE
#endif

#ifdef _MSC_VER
#define IS_INTERFACE __declspec(novtable)
#else
#define IS_INTERFACE
#endif

namespace CharIS {
    // fixed point
    using fp26dot6_t = int32_t;
    using fp58dot6_t = int64_t;
    struct fppoint_t { fp26dot6_t x, y; };
    struct fpsize_t { fp26dot6_t width, height; };

    static inline fp26dot6_t floor(fp26dot6_t x) noexcept { return x & fp26dot6_t(~63); }
    static inline fp26dot6_t ceil(fp26dot6_t x) noexcept { return floor(x + 63); }


    struct Range { uint32_t position, length /*= -1*/; };
    struct RangeToEnd : Range { RangeToEnd(uint32_t p) noexcept { position = p; length = uint32_t(-1); } };



    enum CODE : int32_t {
        CODE_OK = 0,
        CODE_FALSE = 1,
    
        CODE_NOTIMPL        = (int32_t)0x80004001,
        CODE_NOINTERFACE    = (int32_t)0x80004002,
        CODE_POINTER        = (int32_t)0x80004003,
        CODE_ABORT          = (int32_t)0x80004004,
        CODE_FAILED         = (int32_t)0x80004005,
        CODE_UNEXPECTED     = (int32_t)0x8000FFFF,
        CODE_FILE_NOT_FOUND = (int32_t)0x80070002,
        CODE_HANDLE         = (int32_t)0x80070006,
        CODE_INVALIDARG     = (int32_t)0x80070057,
        CODE_OUTOFMEMORY    = (int32_t)0x8007000E,
        CODE_SMALLBUFFER    = (int32_t)0x8007007A,
        CODE_ENDOFSTREAM    = (int32_t)0x80070026,
        CODE_TIMEOUT        = (int32_t)0x8001011F,
        
    };

    inline bool Success(CODE code) noexcept { return int32_t(code) >= 0; }

    inline bool Failure(CODE code) noexcept { return int32_t(code) < 0; }

    struct CHARIS_NO_VTABLE IISBase {

        virtual long Dispose() noexcept = 0;

        virtual long AddRefCnt() noexcept = 0;

    };

    template<typename T>
    inline auto SafeRefCnt(T* p) noexcept { if (p) p->AddRefCnt(); return p; }

    template<typename T>
    inline void SafeDispose(T*& p) noexcept { if (p) { p->Dispose(); p = nullptr; } }

    template<typename T>
    inline auto Take(T& p) noexcept { const auto t = p; p = T{}; return t; }

    template<typename T>
    struct base_ptr {
        base_ptr() noexcept {};
        ~base_ptr() noexcept { SafeDispose(ptr_); }
        base_ptr(const base_ptr<T>& ptr) noexcept : ptr_(SafeRefCnt(ptr.ptr_)) {}
        base_ptr(base_ptr<T>&& ptr) noexcept : ptr_(ptr.ptr_) { ptr.ptr_ = nullptr; }
        base_ptr<T>& operator=(base_ptr<T>&& ptr) noexcept { assert(this != &ptr); std::swap(ptr_, ptr.ptr_); return*this; }
        base_ptr<T>& operator=(const base_ptr<T>& ptr) noexcept = delete;
        T* operator->() const noexcept { return ptr_; }
        T** as_get() noexcept { SafeDispose(ptr_); return &ptr_; };
        T*const* as_set() const noexcept { return &ptr_; };
        operator T*() const noexcept { return ptr_; }
    protected:
        T* ptr_ = nullptr;
    };
    
#ifndef CHARIS_LOGGER_ERR
#define CHARIS_LOGGER_ERR(fmt, ...) std::printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef CHARIS_LOGGER_LOG
//#define CHARIS_LOGGER_LOG(fmt, ...) std::printf("[LOG] " fmt "\n", ##__VA_ARGS__)
#define CHARIS_LOGGER_LOG(fmt, ...) 
#endif
}
