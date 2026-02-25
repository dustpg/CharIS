#define _CRT_SECURE_NO_WARNINGS
#include "os_base.h"
#include <Windows.h>
#include <cstring>
#include <cstdio>
#include <cwchar>

namespace {
enum {
    TIMER_ID_NULL,
    TIMER_ID_BLINK,
};

const wchar_t kWindowClassName[] = L"OsBaseWindowClass";

struct WindowImpl : IDemoWindow {
    HWND hwnd = nullptr;
    WindowConfig config;
    WindowEvents events = {};
    bool quit_requested = false;
    bool draw_caret = true;
    int client_width = 0;
    int client_height = 0;

    void UpdateCaret() {
        draw_caret = !draw_caret;
        if (events.caret_changed)
            events.caret_changed(events.user_data, draw_caret);
    }

    static WindowImpl* FromHandle(HWND h) {
        return reinterpret_cast<WindowImpl*>(::GetWindowLongPtrW(h, GWLP_USERDATA));
    }

    bool Create(const WindowConfig* config, const WindowEvents* events) override {
        if (hwnd) return true;
        if (config) this->config = *config;
        if (events) this->events = *events;
        client_width = this->config.width;
        client_height = this->config.height;

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kWindowClassName;
        if (!::RegisterClassExW(&wc)) {
            std::fprintf(stderr, "RegisterClassExW failed\n");
            return false;
        }

        DWORD style = WS_OVERLAPPEDWINDOW;
        if (!this->config.resizable)
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

        RECT rc = { 0, 0, this->config.width, this->config.height };
        ::AdjustWindowRect(&rc, style, FALSE);
        int win_w = rc.right - rc.left;
        int win_h = rc.bottom - rc.top;

        wchar_t title_buf[256] = {};
        if (this->config.title && this->config.title[0]) {
            int len = ::MultiByteToWideChar(CP_UTF8, 0, this->config.title, -1, title_buf, 256);
            if (len <= 0)
                title_buf[0] = L'\0';
        }
        if (title_buf[0] == 0)
            std::wcsncpy(title_buf, L"Window", 256);

        hwnd = ::CreateWindowExW(
            0,
            kWindowClassName,
            title_buf,
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            win_w, win_h,
            nullptr,
            nullptr,
            ::GetModuleHandleW(nullptr),
            this
        );
        if (!hwnd) {
            std::fprintf(stderr, "CreateWindowExW failed\n");
            return false;
        }

        const auto time = ::GetCaretBlinkTime();
        ::SetTimer(hwnd, TIMER_ID_BLINK, time, nullptr);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        ::ShowWindow(hwnd, SW_SHOWNORMAL);
        return true;
    }

    void* GetNativeHandle() const override {
        return reinterpret_cast<void*>(hwnd);
    }

    WindowSize GetWindowSize() const override {
        WindowSize size;
        size.width = client_width;
        size.height = client_height;
        return size;
    }

    int GetClientWidth() const { return client_width; }
    int GetClientHeight() const { return client_height; }

    bool PumpEvents() override {
        if (!hwnd || quit_requested) return false;
        MSG msg = {};
        if (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                quit_requested = true;
                return false;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            return true;
        }
        return false;
    }

    void RunEventLoop() override {
        if (!hwnd) return;
        MSG msg = {};
        while (!quit_requested && ::GetMessageW(&msg, nullptr, 0, 0) > 0) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    void RequestQuit() override {
        quit_requested = true;
        if (hwnd)
            ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    bool IsQuitRequested() const override {
        return quit_requested;
    }

    void SetTitle(const char* title) override {
        if (!hwnd || !title) return;
        wchar_t buf[384] = {};
        if (::MultiByteToWideChar(CP_UTF8, 0, title, -1, buf, 384) > 0)
            ::SetWindowTextW(hwnd, buf);
    }

    void Destroy() override {
        if (hwnd) {
            ::DestroyWindow(hwnd);
            hwnd = nullptr;
        }
        quit_requested = true;
    }

    void Release() noexcept override {
        Destroy();
        delete this;
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowImpl* w = FromHandle(hwnd);
        if (w) {
            switch (msg) {
            case WM_CLOSE: {
                bool allow = true;
                if (w->events.on_close)
                    allow = w->events.on_close(w->events.user_data);
                if (allow) {
                    w->quit_requested = true;
                    ::DestroyWindow(hwnd);
                }
                return 0;
            }
            case WM_DESTROY:
                w->hwnd = nullptr;
                ::PostQuitMessage(0);
                return 0;
            case WM_TIMER:
                if (wParam == TIMER_ID_BLINK) {
                    w->UpdateCaret();
                }
                return 0;
            case WM_SIZE: {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                w->client_width = width;
                w->client_height = height;
                if (w->events.on_resize)
                    w->events.on_resize(w->events.user_data, width, height);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                const int x = static_cast<int>(static_cast<SHORT>(LOWORD(lParam)));
                const int y = static_cast<int>(static_cast<SHORT>(HIWORD(lParam)));
                if (w->events.on_mouse_left_down)
                    w->events.on_mouse_left_down(w->events.user_data, x, y);
                return 0;
            }
            case WM_LBUTTONUP: {
                const int x = static_cast<int>(static_cast<SHORT>(LOWORD(lParam)));
                const int y = static_cast<int>(static_cast<SHORT>(HIWORD(lParam)));
                if (w->events.on_mouse_left_up)
                    w->events.on_mouse_left_up(w->events.user_data, x, y);
                return 0;
            }
            case WM_MOUSEMOVE:
                if (wParam & MK_LBUTTON) {
                    const int x = static_cast<int>(static_cast<SHORT>(LOWORD(lParam)));
                    const int y = static_cast<int>(static_cast<SHORT>(HIWORD(lParam)));
                    if (w->events.on_mouse_move_with_left)
                        w->events.on_mouse_move_with_left(w->events.user_data, x, y);
                }
                return 0;
            default:
                break;
            }
        } else if (msg == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            w = reinterpret_cast<WindowImpl*>(create->lpCreateParams);
            if (w) {
                w->hwnd = hwnd;
                ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(w));
            }
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

} // namespace

IDemoWindow* CreateDemoWindow() {
    return new WindowImpl();
}
