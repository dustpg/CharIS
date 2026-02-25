#pragma once
#ifdef _MSC_VER
#include <process.h>
#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>
#include <sal.h>
namespace CharIS { namespace impl {
    enum {
        thrd_success = 0,
        thrd_nomem = 1,
        thrd_timedout = 2,
        thrd_busy = 3,
        thrd_error = 4
    };
    using mtx = SRWLOCK;
    using cnd = CONDITION_VARIABLE;
    using thrd = uintptr_t;
    template<typename T>
    inline thrd thrd_create(T* obj) noexcept {
        return _beginthreadex(
            nullptr,
            0,
            [](void* p) noexcept { return (unsigned int)reinterpret_cast<T*>(p)->Run(); },
            obj,
            0,
            nullptr
        );
    }

    inline int mtx_init(mtx* lock) noexcept {
        ::InitializeSRWLock(lock);
        return thrd_success;
    }
    inline void mtx_destroy(mtx*) noexcept {
        // SRWLOCK does not require explicit destruction
    }
    _Acquires_exclusive_lock_(*lock)
    inline int mtx_lock(mtx* lock) noexcept {
        ::AcquireSRWLockExclusive(lock);
        return thrd_success;
    }
    _Requires_lock_held_(lock)
    inline int mtx_unlock(mtx* lock) noexcept {
        ::ReleaseSRWLockExclusive(lock);
        return thrd_success;
    }

    inline void thrd_detach(thrd thread) noexcept {
        ::CloseHandle(reinterpret_cast<HANDLE>(thread)); 
    }
    inline void thrd_exit(int res) noexcept {
        ::_endthreadex(static_cast<unsigned int>(res));
    }

    inline int cnd_init(cnd* cond) noexcept {
        ::InitializeConditionVariable(cond);
        return thrd_success;
    }
    inline void cnd_destroy(cnd*) noexcept {
        // CONDITION_VARIABLE does not require explicit destruction
    }
    inline int cnd_wait(cnd* cond, mtx* mutex) noexcept {
        if (::SleepConditionVariableSRW(cond, mutex, INFINITE, 0)) {
            return thrd_success;
        }
        return thrd_error;
    }
    inline int cnd_signal(cnd* cond) noexcept {
        ::WakeConditionVariable(cond);
        return thrd_success;
    }
    inline int cnd_broadcast(cnd* cond) noexcept {
        ::WakeAllConditionVariable(cond);
        return thrd_success;
    }
    inline int thrd_join(thrd thread, int* res) noexcept {
        const auto handle = reinterpret_cast<HANDLE>(thread);
        if (::WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0) {
            return thrd_error;
        }
        if (res) {
            DWORD exit_code = 0;
            if (!::GetExitCodeThread(handle, &exit_code)) {
                return thrd_error;
            }
            *res = static_cast<int>(exit_code);
        }
        ::CloseHandle(handle);
        return thrd_success;
    }
}}

#else
#include <pthread.h>
#include <unistd.h>
namespace CharIS { namespace impl {
    using thrd = /*impl*/;
}}
#endif