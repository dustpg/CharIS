#pragma once

#include <charis/include/is_base.h>

namespace CharIS {

    struct Node {

        Node* prev, * next;

        void Insert(Node* node) noexcept {
            this->next->prev = node;
            node->next = this->next;
            node->prev = this;
            this->next = node;
        }

        void Remove() noexcept {
            this->next->prev = this->prev;
            this->prev->next = this->next;
        }

    };


    template<typename T>
    inline void SafeRelease(T*& p) noexcept { if (p) { p->Release(); p = nullptr; } }

    // windows HRESULT
    inline CODE HResult(long c) noexcept { return static_cast<CODE>(c); }

}
