#pragma once
#include <cstdint>
#include "is_function.h"


namespace CharIS {

    class CISThreadPool {
        using Func = impl::func_node<void()>;
        struct Impl;
        enum { MAX_THREAD = 64 };
    public:

        CISThreadPool() noexcept;

        ~CISThreadPool() noexcept;

        bool Init(uint32_t threads = 1) noexcept;

        void Shutdown() noexcept;

        template<typename T>
        bool Add(T&& lambda) noexcept {
            if (!m_pImpl) return false;
            using FuncType = std::decay_t<T>;
            auto* callable = new(std::nothrow) impl::func_storage<void(), FuncType>(std::forward<T>(lambda));
            if (!callable) {
                return false;
            }
            return push(callable);
        }

    protected:

        bool push(Func*) noexcept;

        Impl*           m_pImpl = nullptr;
    
    };


}