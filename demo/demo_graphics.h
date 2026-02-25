#pragma once
#include <CharIS/include/is_graphics_api.h>
#include <vector>
using CharIS::CODE;

struct IDemoGraphics : CharIS::IISGraphics {

    virtual CODE Initialize(void* hwnd) = 0;

    virtual void Shutdown() = 0;

    virtual void Release() = 0;

    virtual void* GetDeviceContext() const = 0;

    virtual void BeginFrame(std::vector<uint64_t>& ids) = 0;

    virtual void EndFrame() = 0;

    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

    virtual void Clear(float r, float g, float b, float a) = 0;

};

extern "C" IDemoGraphics* CreateDemoGraphicsD3D11() ;
