#include "is_thread_pool.h"
#include "is_pod_queue.h"
#include "is_thread_impl.h"

namespace CharIS {

    namespace impl {

        struct task {
            impl::func_node<void()>* callable;
        };

    }

    struct CISThreadPool::Impl {

        Impl(uint32_t t) noexcept;

        ~Impl() noexcept;

        int Run() noexcept;

        auto IsExiting() const noexcept { return m_idTask; }

        auto GetCount() const noexcept { return m_uThreadCount; }

        bool Push(impl::func_node<void()>*) noexcept;

        bool PushZero(impl::func_node<void()>*) noexcept;

        void Shutdown() noexcept;

    protected:

        pod::queue<impl::task>  m_qTask;

    protected:

        impl::mtx               m_mtxLock = { };

        impl::cnd               m_cndNotify = {};

        uint32_t                m_uThreadCount = 0;

        uint32_t                m_uThreadStarted = 0;
        // exit flag, TODO: TASK ID
        size_t                  m_idTask = 0;

        impl::thrd              m_szThreads[1] = { };
        // more after here
    };


    CISThreadPool::Impl::Impl(uint32_t t) noexcept
    {
        int code = impl::thrd_success;

        code = impl::mtx_init(&m_mtxLock);
        if (code != impl::thrd_success)
            return;

        code = impl::cnd_init(&m_cndNotify);
        if (code != impl::thrd_success)
            return;

        uint32_t i;
        for (i = 0; i != t; ++i) {
            const auto thr = impl::thrd_create(this);
            if (!thr) break;
            m_szThreads[i] = thr;
        }
        m_uThreadCount = i;
        m_uThreadStarted = i;
    }

    CISThreadPool::Impl::~Impl() noexcept
    {
        const auto count = m_uThreadCount;
        m_uThreadCount = 0;
        if (count) this->Shutdown();

        for (uint32_t i = 0; i != count; ++i) {
            const auto thr = m_szThreads[i];
            impl::thrd_join(thr, nullptr);
        }
        impl::cnd_destroy(&m_cndNotify);
        impl::mtx_destroy(&m_mtxLock);
    }

    void CISThreadPool::Impl::Shutdown() noexcept
    {
        if (impl::mtx_lock(&m_mtxLock) != impl::thrd_success) {
            m_idTask = 1;
            return;
        }

        // Critical section
        m_idTask = 1;
        impl::cnd_broadcast(&m_cndNotify);


        impl::mtx_unlock(&m_mtxLock);
    }

    bool CISThreadPool::Impl::PushZero(impl::func_node<void()>* obj) noexcept
    {
        impl::task t = { obj };
        return m_qTask.push(t);
    }

    bool CISThreadPool::Impl::Push(impl::func_node<void()>* obj) noexcept
    {
        if (impl::mtx_lock(&m_mtxLock) != impl::thrd_success) {
            return false;
        }

        // Critical section
        bool rv = [this, obj]() noexcept {

            // exiting
            if (this->IsExiting())
                return false;

            impl::task task = { obj };
            // full
            if (!m_qTask.push(task))
                return false;

            // notify
            if (impl::cnd_signal(&m_cndNotify) != impl::thrd_success)
                return false;
        
            return true;
        }();
        

        if (impl::mtx_unlock(&m_mtxLock) != impl::thrd_success) {
            rv = false;
        }

        return rv;
    }

    int CISThreadPool::Impl::Run() noexcept
    {
        //printf("TID = %d\n", int(GetCurrentThreadId()));

        while (true) {
            impl::mtx_lock(&m_mtxLock);
        
            while (m_qTask.empty() && !this->IsExiting()) {
                impl::cnd_wait(&m_cndNotify, &m_mtxLock);
            }

            if (this->IsExiting() && m_qTask.empty())
                break;

            auto task = m_qTask.front();
            m_qTask.pop();
            impl::mtx_unlock(&m_mtxLock);
            task.callable->invoke();
            task.callable->cleanup();
        }

        m_uThreadStarted--;
        impl::mtx_unlock(&m_mtxLock);
        impl::thrd_exit(0);
        return 0;
    }
}


CharIS::CISThreadPool::CISThreadPool() noexcept
{

}

CharIS::CISThreadPool::~CISThreadPool() noexcept
{
    delete m_pImpl;
    m_pImpl = nullptr;
}

void CharIS::CISThreadPool::Shutdown() noexcept
{
    delete m_pImpl;
    m_pImpl = nullptr;
}

bool CharIS::CISThreadPool::push(Func* func) noexcept
{
    assert(m_pImpl && func);
    if (m_pImpl->GetCount()) {
        if (m_pImpl->Push(func))
            return true;
    }
    else {
        if (m_pImpl->PushZero(func))
            return true;
    }
    func->cleanup();
    return false;
}

bool CharIS::CISThreadPool::Init(uint32_t threads) noexcept
{
    assert(m_pImpl == nullptr);
    if (m_pImpl)
        return true;
    assert(threads <= MAX_THREAD);
    if (threads > MAX_THREAD)
        return false;

    const auto bytes = threads
        ? size_t(threads - 1) * sizeof(impl::thrd) + sizeof(Impl)
        : sizeof(Impl)
        ;

    if (const auto ptr = charis_malloc(bytes)) {
        const auto obj = new(ptr) Impl{ threads };
        if (threads == 0 || obj->GetCount()) {
            m_pImpl = obj;
            return true;
        }
        obj->~Impl();
        charis_free(ptr);
    }
    return false;
}