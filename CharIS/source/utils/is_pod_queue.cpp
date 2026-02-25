#include "is_pod_queue.h"
#include "../core/is_helper_c.h"
#include <cstring>
#include <cassert>

namespace CharIS {
    enum : uint32_t { INIT_POD_QUEUE_MINSIZE = 16 };
}

using namespace CharIS::impl;

queue_base::queue_base() noexcept
{

}

void* queue_base::push(size_t sz) noexcept
{
    assert(sz && "bad size");
    const auto size = this->size();
    // FULL
    if (size == m_uCapacity) {
        const uint32_t newCap = m_uCapacity ? m_uCapacity * 2 : INIT_POD_QUEUE_MINSIZE;
        const size_t fullBytes = sz * size_t(newCap);
        const auto ptr = charis_realloc(m_pData, fullBytes);
        if (!ptr)
            return nullptr;

        const auto base = static_cast<char*>(ptr);
        m_pData = base;

        if (m_uCapacity) {
            const auto halfBytes = fullBytes / 2;
            std::memcpy(base + halfBytes, base, halfBytes);
        }
        m_uCapacity = newCap;
    }
    const auto rv = this->back(sz);
    m_uTail++;
    return rv;
}

bool queue_base::push_cpy(const void* data, size_t sz) noexcept
{
    if (const auto ptr = this->push(sz)) {
        std::memcpy(ptr, data, sz);
    }
    return false;
}

void queue_base::pop() noexcept
{
    assert(!empty());
    m_uHead++;
}

queue_base::~queue_base() noexcept
{
    charis_free(m_pData);
    m_pData = nullptr;
    m_uCapacity = 0;
    m_uHead = 0;
    m_uTail = 0;
}