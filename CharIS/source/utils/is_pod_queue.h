#pragma once

#include <cstdint>
#include <type_traits>
#include <cassert>

namespace CharIS { namespace impl {

    struct queue_base {

        queue_base() noexcept;

        ~queue_base() noexcept;

        bool empty() const { return m_uHead == m_uTail; }

        auto size() const noexcept -> uint32_t { return m_uTail - m_uHead; }

        bool push_cpy(const void* data, size_t sz) noexcept;

        void*push(size_t sz) noexcept;

        void pop() noexcept;

        auto front(size_t sz) const noexcept { return m_pData + sz * size_t(m_uHead & uint32_t(m_uCapacity - 1)); }

        auto back(size_t sz) const noexcept { return m_pData + sz * size_t(m_uTail & uint32_t(m_uCapacity - 1)); }

    protected:

        char*           m_pData = nullptr;
        // MUST BE POWER OF 2 ( OR 0)
        uint32_t        m_uCapacity = 0;
        // head % size = front
        uint32_t        m_uHead = 0;
        // head % size = back
        uint32_t        m_uTail = 0;
    };

    // Modern POD check (replacement for deprecated std::is_pod)
    template<typename T>
    struct queue_is_pod_like : std::bool_constant<
        std::is_trivially_copyable_v<T> && 
        std::is_standard_layout_v<T>
    > {};

    template<typename T>
    constexpr bool queue_is_pod_like_v = queue_is_pod_like<T>::value;
}}




namespace CharIS { namespace pod {

    // Type-erased POD queue
    template<typename T> 
    class queue : protected impl::queue_base {
        // Type helper
        static inline auto tr(T* ptr) noexcept -> char* { return reinterpret_cast<char*>(ptr); }
        // Type helper
        static inline auto tr(const T* ptr) noexcept -> const char* { return reinterpret_cast<const char*>(ptr); }

    public:
        // Size type
        using size_type = uint32_t;

        // Check for POD-like (modern replacement for is_pod)
        static_assert(impl::queue_is_pod_like_v<T>, "type T must be POD-like (trivially copyable and standard layout)");

        // Ctor
        queue() noexcept = default;
        // Copy ctor (deleted - queue_base doesn't support copy)
        queue(const queue&) = delete;
        // Move ctor (deleted - queue_base doesn't support move)
        queue(queue&&) = delete;
        // Copy operator= (deleted)
        queue& operator=(const queue&) = delete;
        // Move operator= (deleted)
        queue& operator=(queue&&) = delete;

        // Is empty?
        bool empty() const noexcept { return queue_base::empty(); }
        // Size of queue
        auto size() const noexcept -> size_type { return queue_base::size(); }
        // Push back (returns false on failure)
        bool push(const T& x) noexcept { 
            if constexpr (sizeof(T) > sizeof(size_t)) {
                return queue_base::push_cpy(tr(&x), sizeof(T));
            }
            else {
                if (const auto ptr = static_cast<T*>(queue_base::push(sizeof(T)))) {
                    *ptr = x;
                    return true;
                }
                return false;
            }

        }
        // Pop front
        void pop() noexcept { assert(!empty() && "UB: empty queue"); queue_base::pop(); }
        // Get front
        auto front() noexcept -> T& { assert(!empty() && "UB: empty queue"); return *reinterpret_cast<T*>(const_cast<char*>(queue_base::front(sizeof(T)))); }
        // Get front const
        auto front() const noexcept -> const T& { assert(!empty() && "UB: empty queue"); return *reinterpret_cast<const T*>(queue_base::front(sizeof(T))); }
        // Get back (last element, not next insertion position)
        auto back() noexcept -> T& { 
            assert(!empty() && "UB: empty queue"); 
            const auto tail_minus_one = (m_uTail - 1) & uint32_t(m_uCapacity - 1);
            return *reinterpret_cast<T*>(const_cast<char*>(m_pData + sizeof(T) * size_t(tail_minus_one))); 
        }
        // Get back const (last element, not next insertion position)
        auto back() const noexcept -> const T& { 
            assert(!empty() && "UB: empty queue"); 
            const auto tail_minus_one = (m_uTail - 1) & uint32_t(m_uCapacity - 1);
            return *reinterpret_cast<const T*>(m_pData + sizeof(T) * size_t(tail_minus_one)); 
        }
    };

}}