#pragma once
#include <atomic>
#include <new>
#include "is_helper_c.h"
#include <charis/include/is_base.h>

namespace CharIS {

    inline fp26dot6_t mul(fp26dot6_t a, fp26dot6_t b) noexcept {
        const int64_t rv = (int64_t(a) * int64_t(b)) >> 6;
        return static_cast<fp26dot6_t>(rv);
    }

    inline fp58dot6_t mul64(fp58dot6_t a, fp58dot6_t b) noexcept {
        const int64_t rv = (a * b) >> 6;
        return rv;
    }

    namespace impl {

        template<typename T>
        struct weak { T* ptr; };

        template<typename T>
        struct relaxed_counter {
            explicit relaxed_counter(T v) noexcept : count_(v) {}
            relaxed_counter(const relaxed_counter&) noexcept = delete;
            relaxed_counter& operator=(const relaxed_counter&) noexcept = delete;
            
            // Increment operators
            T operator++() noexcept { return count_.fetch_add(1, std::memory_order_relaxed) + 1; }
            T operator++(int) noexcept { return count_.fetch_add(1, std::memory_order_relaxed); }
            
            // Decrement operators
            T operator--() noexcept { return count_.fetch_sub(1, std::memory_order_acq_rel) - 1; }
            T operator--(int) noexcept { return count_.fetch_sub(1, std::memory_order_acq_rel ); }
            
        private:
            std::atomic<T>      count_;
        };
    
        template<typename T = impl::relaxed_counter<int32_t>>
        struct ref_count {

            long add_ref() noexcept { return ++ref_count_; };

            template<typename T>
            long dispose(T* p) noexcept { 
                const auto cnt = --ref_count_;
                if (!cnt) delete p;
                return cnt; 
            }

            template<typename T>
            long dispose_move(T* p) noexcept {
                const auto cnt = --ref_count_;
                if (!cnt) {
                    if (p->Move()) {
                        ++ref_count_;
                        return 1;
                    }
                    else {
                        delete p;
                        return 0;
                    }
                };
                return cnt;
            }

            T           ref_count_{ 1 };

        };

        struct base_class {

            // Override new/delete to use imalloc/ifree
            static void* operator new(size_t size, const std::nothrow_t&) noexcept { return charis_malloc(size); }
            static void operator delete(void* ptr) noexcept { charis_free(ptr); }
            static void operator delete(void* ptr, const std::nothrow_t&) noexcept { charis_free(ptr); }
        };

        struct base_class_replacement {
            //uint32_t    base_class_magic = 0x44524849;
            static void* operator new(size_t size) = delete;
            static void operator delete(void* ptr) noexcept { charis_free(ptr); }
            static void* operator new(size_t size, void* ptr) noexcept { return ptr; }

        };

        // I->std::has_virtual_destructor_v || std::is_final
        template<typename T, typename I>
        class ref_count_class : public I, public base_class {
        public:

            long Dispose() noexcept override final { return counter_.dispose(static_cast<T*>(this)); }

            long AddRefCnt() noexcept override final { return counter_.add_ref(); }

        protected:

            ref_count<>         counter_;

        };

        // I->std::has_virtual_destructor_v || std::is_final
        template<typename T, typename I>
        class ref_count_move : public I, public base_class {
        public:

            long Dispose() noexcept override final { return counter_.dispose_move(static_cast<T*>(this)); }

            long AddRefCnt() noexcept override final { return counter_.add_ref(); }

        protected:

            ref_count<>         counter_;

        };

    }

}