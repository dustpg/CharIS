#pragma once

#include <cstddef>

struct WindowConfig {
    const char* title = "Window";
    int width = 800 * 2;
    int height = 600 * 2;
    bool resizable = false;
};

struct WindowEvents {
    bool (*on_close)(void* user_data) = nullptr;
    void (*on_resize)(void* user_data, int width, int height) = nullptr;
    void (*caret_changed)(void* user_data, bool) = nullptr;
    void (*on_mouse_left_down)(void* user_data, int x, int y) = nullptr;
    void (*on_mouse_left_up)(void* user_data, int x, int y) = nullptr;
    void (*on_mouse_move_with_left)(void* user_data, int x, int y) = nullptr;
    void* user_data = nullptr;
};

struct WindowSize {
    int width, height;
};

struct IDemoWindow {

    virtual bool Create(const WindowConfig* config, const WindowEvents* events) = 0;

    virtual void* GetNativeHandle() const = 0;

    virtual auto GetWindowSize() const -> WindowSize = 0;

    virtual bool PumpEvents() = 0;

    virtual void RunEventLoop() = 0;

    virtual void RequestQuit() = 0;

    virtual bool IsQuitRequested() const = 0;

    /** Set window title (UTF-8). No-op if title is null. */
    virtual void SetTitle(const char* title) = 0;

    virtual void Destroy() = 0;

    virtual void Release() noexcept = 0;

};

IDemoWindow* CreateDemoWindow();
