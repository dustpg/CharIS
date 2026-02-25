#pragma once
#include <utility>
#include <type_traits>
#include <cassert>
#include "../core/is_helper_p.h"

namespace CharIS {

    namespace impl {

        template<typename Signature>
        struct func_node;

        template<typename R, typename... Args>
        struct func_node<R(Args...)> : base_class {
            using InvokeFn = std::conditional_t<
                std::is_void_v<R>,
                void(*)(void*, Args...) noexcept,
                R(*)(void*, Args...) noexcept
            >;
            using CleanupFn = void(*)(void*) noexcept;

            InvokeFn invoke_fn;
            CleanupFn cleanup_fn;

            func_node(InvokeFn inv, CleanupFn cf) noexcept : invoke_fn(inv), cleanup_fn(cf) {}

            R invoke(Args... args) const noexcept {
                return invoke_fn(const_cast<func_node*>(this), std::forward<Args>(args)...);
            }

            void cleanup() noexcept {
                if (cleanup_fn)
                    cleanup_fn(const_cast<func_node*>(this));
                operator delete(const_cast<func_node*>(this), std::nothrow);
            }
        };

        // Concrete storage for callable
        template<typename Signature, typename F>
        struct func_storage;

        template<typename R, typename... Args, typename F>
        struct func_storage<R(Args...), F> : func_node<R(Args...)> {
        private:
            F f;

            static R invoke_impl(void* self, Args... args) noexcept {
                auto* c = static_cast<func_storage<R(Args...), F>*>(self);
                return c->f(std::forward<Args>(args)...);
            }

            static void cleanup_impl(void* self) noexcept {
                auto* c = static_cast<func_storage<R(Args...), F>*>(self);
                c->~func_storage();
            }

        public:
            static constexpr typename func_node<R(Args...)>::CleanupFn get_cleanup_fn() noexcept {
                return std::is_trivially_destructible_v<F> ? nullptr : cleanup_impl;
            }

            explicit func_storage(const F& func)
                : func_node<R(Args...)>{invoke_impl, get_cleanup_fn()}, f(func) {}

            explicit func_storage(F&& func)
                : func_node<R(Args...)>{invoke_impl, get_cleanup_fn()}, f(std::move(func)) {}
        };

    }

    template<typename Signature>
    class function;

    template<typename R, typename... Args>
    class function<R(Args...)> {
    private:
        impl::func_node<R(Args...)>* ptr_ = nullptr;

        void cleanup() noexcept {
            if (ptr_) {
                ptr_->cleanup();
                ptr_ = nullptr;
            }
        }

    public:
        function() noexcept = default;

        function(std::nullptr_t) noexcept {}

        function(function&& other) noexcept : ptr_(other.ptr_) {
            other.ptr_ = nullptr;
        }

        template<typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function<R(Args...)>>>>
        function(F&& f) {
            using decayed = std::decay_t<F>;
            if constexpr (std::is_pointer_v<decayed> || std::is_member_pointer_v<decayed>) {
                if (f) {
                    ptr_ = new(std::nothrow) impl::func_storage<R(Args...), decayed>(std::forward<F>(f));
                }
            } else {
                ptr_ = new(std::nothrow) impl::func_storage<R(Args...), decayed>(std::forward<F>(f));
            }
        }

        ~function() noexcept { cleanup(); }

        function(const function&) = delete;
        function& operator=(const function&) = delete;

        function& operator=(function&& other) noexcept {
            if (this != &other) {
                cleanup();
                ptr_ = other.ptr_;
                other.ptr_ = nullptr;
            }
            return *this;
        }

        function& operator=(std::nullptr_t) noexcept {
            cleanup();
            return *this;
        }

        template<typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function<R(Args...)>>>>
        function& operator=(F&& f) {
            cleanup();
            *this = function(std::forward<F>(f));
            return *this;
        }

        template<typename... ForwardArgs>
        R operator()(ForwardArgs&&... args) const noexcept {
            assert(ptr_ && "function: call on empty");
            return ptr_->invoke_fn(ptr_, std::forward<ForwardArgs>(args)...);
        }

        explicit operator bool() const noexcept {
            return ptr_ != nullptr;
        }

        void swap(function& other) noexcept {
            std::swap(ptr_, other.ptr_);
        }
    };

    template<typename R, typename... Args>
    void swap(function<R(Args...)>& a, function<R(Args...)>& b) noexcept {
        a.swap(b);
    }

}
