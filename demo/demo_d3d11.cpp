#include "demo_graphics.h"
#include <cassert>
#include <cstdint>
#include <new>
#include <mutex>
#include <vector>
#include <cstdio>
#include <cstddef>

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <DirectXMath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using CharIS::CODE;
using CharIS::DrawTextureParam;
using CharIS::TextEffect;
using CharIS::fppoint_t;
using CharIS::fpsize_t;

struct AffineMatrix2D {
    DirectX::XMFLOAT4 m12;    // [m11, m21, m12, m22]
    DirectX::XMFLOAT4 dxy;    // [dx, dy, -, -]
    DirectX::XMFLOAT4 screen; // [w, h, 1/w, 1/h]
    DirectX::XMFLOAT4 texParams; // [w, h, 1/w, 1/h]
};

// Batch 渲染相关常量
#ifdef NDEBUG
constexpr uint32_t TEXT_BATCH_COUNT = 1024;
#else
constexpr uint32_t TEXT_BATCH_COUNT = 64;
#endif

// 顶点结构定义
struct MsdfVertex {
    DirectX::XMFLOAT2 pos;
    DirectX::XMFLOAT4 tex;
    uint32_t color1;
    uint32_t color2;
};

// 灰度实例化：单位四边形顶点（仅 4 个顶点复用）
struct GrayQuadVertex {
    DirectX::XMFLOAT2 pos;   // 单位矩形 0~1
    DirectX::XMFLOAT2 tex;   // 单位 UV 0~1
};
// 每个灰度矩形一条实例数据（模仿 Msdf：fp26.6 dst + 16bit 纹理像素坐标，UV 在 GPU 计算）
struct GrayInstanceData {
    int32_t dstX, dstY, dstW, dstH;       // 原始 fp26.6
    uint16_t srcX, srcY, srcW, srcH;      // 纹理内像素矩形，16 位（省带宽）
    uint32_t color;
};
// 每个MSDF矩形一条实例数据（dst 为原始 fp26.6；src 为像素 16 位，在 HLSL 中再转归一化 UV）
struct MsdfInstanceData {
    int32_t dstX, dstY, dstW, dstH;       // 原始 fp26.6
    uint16_t srcX, srcY, srcW, srcH;      // 纹理内像素矩形，16 位足够（省 8 字节）
    uint32_t color1;
    uint32_t color2;
    int32_t size;  // fp26.6
};

struct RectVertex {
    DirectX::XMFLOAT2 pos;
    uint32_t color;
};
// 纯色矩形实例化：单位四边形顶点（仅位置）
struct RectQuadVertex {
    DirectX::XMFLOAT2 pos;
};
// 每个纯色矩形一条实例数据
struct RectInstanceData {
    DirectX::XMFLOAT4 rect;  // x, y, w, h
    uint32_t color;
};

struct PolygonVertex {
    DirectX::XMFLOAT2 pos;
    DirectX::XMFLOAT2 nor;
    uint32_t index;
};

namespace {
    template<typename T>
    void SafeRelease(T& p) noexcept {
        if (p) {
            p->Release();
            p = nullptr;
        }
    }
}


// D3D11 实现的图形接口
class DemoGraphicsD3D11 : public IDemoGraphics {
public:
    DemoGraphicsD3D11() = default;

    ~DemoGraphicsD3D11() { Shutdown(); }

    // IDemoGraphics 接口实现
    CODE Initialize(void* hwnd) noexcept override;
    void Shutdown() noexcept override;
    void* GetDeviceContext() const noexcept override;
    void Release() noexcept override;

    // IISGraphics 接口实现
    void DisposeTexture(uint64_t handle) noexcept override;
    CODE CreateAltasTexture(const CharIS::CreateTextureParam param, uint64_t& handle) noexcept override;
    void DrawAltas(const DrawTextureParam& draw, const TextEffect& effect, void* context) noexcept override;
    void Upload(uint64_t handle, uint64_t id, CharIS::Box2D box, const void* data, uint32_t pitch) noexcept override;
    void FillRect(uint32_t background, void* context, fppoint_t point, fpsize_t size) noexcept override;

public:

    void BeginFrame(std::vector<uint64_t>&) noexcept override;
    void EndFrame() noexcept override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept override;
    void Clear(float r, float g, float b, float a) noexcept override;
    void DrawGray(const DrawTextureParam& draw, const TextEffect& effect);
    void DrawSubpixel(const DrawTextureParam& draw, const TextEffect& effect);
    void DrawMsdf(const DrawTextureParam& draw, const TextEffect& effect);
    void FillRect(float x, float y, float width, float height, uint32_t color);
    
    // Batch 渲染相关方法
    CODE InitializeBatchRendering() noexcept;
    void ShutdownBatchRendering() noexcept;
    void Flush() noexcept;  // 执行 batch 渲染
    void FlushGray() noexcept;  // 执行 gray batch 渲染
    void FlushSubpixel() noexcept;  // 执行 subpixel batch 渲染（RGB 三通道 blend）
    void FlushMsdf() noexcept;      // 执行 msdf batch 渲染
    void FlushRect() noexcept;
    
private:
    HWND hwnd_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11DeviceContext1* context_upload_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* renderTargetView_ = nullptr;
    ID3D11Texture2D* backBuffer_ = nullptr;
    ID3D11Debug* debug_ = nullptr;
    D3D11_VIEWPORT viewport_ = {};
    std::vector<uint64_t> tasks_;
    std::mutex upload_mtx_;
    bool initialized_ = false;
    
    // Batch 渲染资源
    //ID3D11Buffer* vertexBuffer_ = nullptr;
    //ID3D11Buffer* batchVertexBuffer_ = nullptr;
    //ID3D11Buffer* indexBuffer_ = nullptr;
    
    // Gray 实例化渲染资源：单位四边形 VB、6 索引 IB、实例 VB
    ID3D11Buffer* grayQuadVertexBuffer_ = nullptr;
    ID3D11Buffer* grayInstanceBuffer_ = nullptr;

    // Subpixel 实例化渲染资源：复用 Gray 单位四边形 VB，单独实例 VB
    ID3D11Buffer* subpixelInstanceBuffer_ = nullptr;
    
    // Rect Fill 实例化渲染资源：单位四边形 VB、实例 VB（与灰度类似）
    ID3D11Buffer* rectQuadVertexBuffer_ = nullptr;
    ID3D11Buffer* rectInstanceBuffer_ = nullptr;

    // MSDF 实例化渲染资源：复用 Gray 单位四边形 VB，单独实例 VB
    ID3D11Buffer* msdfInstanceBuffer_ = nullptr;
    
    // Shaders
    //ID3D11VertexShader* rectVs_ = nullptr;
    //ID3D11VertexShader* rectVsPixel_ = nullptr;
    ID3D11VertexShader* grayVs_ = nullptr;
    //ID3D11VertexShader* rectFillVs_ = nullptr;
    ID3D11VertexShader* rectFillInstancedVs_ = nullptr;
    ID3D11VertexShader* msdfInstancedVs_ = nullptr;
    ID3D11PixelShader* msdfPs_ = nullptr;
    ID3D11PixelShader* grayPs_ = nullptr;
    ID3D11PixelShader* subpixelPs_ = nullptr;
    ID3D11PixelShader* rectFillPs_ = nullptr;
    //ID3D11PixelShader* colorPs_ = nullptr;
    //ID3D11VertexShader* polygonVs_ = nullptr;
    //ID3D11PixelShader* polygonPs_ = nullptr;
    //ID3D11GeometryShader* extrudeGs_ = nullptr;
    
    // Input Layouts
    //ID3D11InputLayout* rectLayout_ = nullptr;
    ID3D11InputLayout* grayLayout_ = nullptr;
    //ID3D11InputLayout* rectFillLayout_ = nullptr;
    ID3D11InputLayout* rectFillInstancedLayout_ = nullptr;
    ID3D11InputLayout* msdfLayout_ = nullptr;
    //ID3D11InputLayout* polygonLayout_ = nullptr;
    
    // Constant Buffer 和 Sampler
    ID3D11Buffer* constantBuffer_ = nullptr;
    ID3D11SamplerState* samplerStatePoint_ = nullptr;   // Point：灰度和子像素
    ID3D11SamplerState* samplerStateLinear_ = nullptr;  // Linear：(M)SDF
    ID3D11RasterizerState* rasterizerState_ = nullptr;
    ID3D11BlendState* blendState_ = nullptr;
    ID3D11BlendState* blendStateSubpixel_ = nullptr;  // RGB 三通道 blend (SRC1/INV_SRC1)
    
    // Batch 状态
    //uint32_t currentBatchCount_ = 0;
    //std::vector<MsdfVertex> batchVertices_;
    //uint64_t currentTextureHandle_ = 0;
    bool batchRenderingInitialized_ = false;
    
    // Gray 实例化状态
    uint32_t grayCurrentBatchCount_ = 0;
    std::vector<GrayInstanceData> grayInstanceData_;
    uint64_t grayCurrentTextureHandle_ = 0;

    // Subpixel 实例化状态（与 Gray 同布局，纹理为 R8G8B8 子像素）
    uint32_t subpixelCurrentBatchCount_ = 0;
    std::vector<GrayInstanceData> subpixelInstanceData_;
    uint64_t subpixelCurrentTextureHandle_ = 0;
    
    // Rect Fill 实例化状态
    uint32_t rectFillCurrentBatchCount_ = 0;
    std::vector<RectInstanceData> rectInstanceData_;

    // MSDF 实例化状态（color1/color2/range）
    uint32_t msdfCurrentBatchCount_ = 0;
    std::vector<MsdfInstanceData> msdfInstanceData_;
    uint64_t msdfCurrentTextureHandle_ = 0;
};

// 实现方法

CharIS::CODE DemoGraphicsD3D11::Initialize(void* hwnd) noexcept {
    if (initialized_) {
        return CharIS::CODE_FAILED;
    }

    hwnd_ = static_cast<HWND>(hwnd);
    if (!hwnd_) {
        return CharIS::CODE_INVALIDARG;
    }

    HRESULT hr = S_OK;

    // 创建交换链描述
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd_;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;

    // 创建设备和交换链
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;
    UINT createDeviceFlags = 0;
#ifndef NDEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &swapChain_,
        &device_,
        &featureLevel,
        &context_
    );

    if (FAILED(hr)) {
        return CharIS::CODE_FAILED;
    }

    // 获取后备缓冲区
    hr = swapChain_->GetBuffer(0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&backBuffer_));
    if (FAILED(hr)) {
        Shutdown();
        return CharIS::CODE_FAILED;
    }

    // 创建渲染目标视图
    hr = device_->CreateRenderTargetView(backBuffer_, nullptr, &renderTargetView_);
    if (FAILED(hr)) {
        Shutdown();
        return CharIS::CODE_FAILED;
    }

    // 设置渲染目标
    context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);

    // 创建延迟上下文，供上传线程使用（仅在该线程中录制命令）
    ID3D11DeviceContext* deferredContext = nullptr;
    hr = device_->CreateDeferredContext(0, &deferredContext);
    if (FAILED(hr)) {
        Shutdown();
        return CharIS::CODE_FAILED;
    }
    
    // 查询 ID3D11DeviceContext1 接口以使用 UpdateSubresource1
    hr = deferredContext->QueryInterface(IID_PPV_ARGS(&context_upload_));
    deferredContext->Release();
    if (FAILED(hr)) {
        Shutdown();
        return CharIS::CODE_FAILED;
    }

    // 设置默认视口
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    SetViewport(0, 0, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

    // 获取调试接口
#ifndef NDEBUG
    hr = device_->QueryInterface(IID_PPV_ARGS(&debug_));
    if (FAILED(hr)) {
        debug_ = nullptr;
    }
#endif


    hr = this->InitializeBatchRendering();

    if (FAILED(hr)) {
        Shutdown();
        return CharIS::CODE_FAILED;
    }

    

    initialized_ = true;
    return CharIS::CODE_OK;
}

void DemoGraphicsD3D11::Shutdown() noexcept {
    ShutdownBatchRendering();
    
    if (renderTargetView_) {
        renderTargetView_->Release();
        renderTargetView_ = nullptr;
    }
    if (backBuffer_) {
        backBuffer_->Release();
        backBuffer_ = nullptr;
    }
    if (swapChain_) {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
    if (context_upload_) {
        context_upload_->Release();
        context_upload_ = nullptr;
    }
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }

    // 报告未释放的 D3D 对象
    if (debug_) {
        //debug_->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        debug_->Release();
        debug_ = nullptr;
    }


    initialized_ = false;
    hwnd_ = nullptr;
}

void DemoGraphicsD3D11::BeginFrame(std::vector<uint64_t>& tasks) noexcept {
    if (context_upload_ && context_) {
        HRESULT hr;
        ID3D11CommandList* cmdList = nullptr;
        tasks.clear();
        upload_mtx_.lock();
        tasks.swap(tasks_);
        hr = context_upload_->FinishCommandList(FALSE, &cmdList);
        upload_mtx_.unlock();
        if (SUCCEEDED(hr)) {
            context_->ExecuteCommandList(cmdList, FALSE);
            cmdList->Release();
        }
    }
    
    // 在每帧开始时更新 constant buffer
    if (constantBuffer_ && context_) {
        D3D11_MAPPED_SUBRESOURCE cbMapped;
        HRESULT hr = context_->Map(constantBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
        if (SUCCEEDED(hr)) {
            
            AffineMatrix2D* cbData = static_cast<AffineMatrix2D*>(cbMapped.pData);
            cbData->m12 = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);  // 单位矩阵
            cbData->dxy = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            float screenW = viewport_.Width;
            float screenH = viewport_.Height;
            cbData->screen = DirectX::XMFLOAT4(screenW, screenH, 1.0f / screenW, 1.0f / screenH);
            // TODO: CHANGE SIZE
            cbData->texParams = DirectX::XMFLOAT4(2048.f, 2048.f, 1.0f / 2048.f, 1.0f / 2048.f);
            
            context_->Unmap(constantBuffer_, 0);
        }
    }
}

void DemoGraphicsD3D11::EndFrame() noexcept {
    Flush();  // 在帧结束时 flush 所有 batch
    swapChain_->Present(1, 0);
}

void DemoGraphicsD3D11::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept {
    if (!initialized_ || !context_) return;
    
    viewport_.TopLeftX = static_cast<float>(x);
    viewport_.TopLeftY = static_cast<float>(y);
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    
    context_->RSSetViewports(1, &viewport_);
}

void DemoGraphicsD3D11::Clear(float r, float g, float b, float a) noexcept {
    if (!initialized_ || !context_ || !renderTargetView_) return;
    
    float color[4] = { r, g, b, a };
    context_->ClearRenderTargetView(renderTargetView_, color);
}

void* DemoGraphicsD3D11::GetDeviceContext() const noexcept {
    return context_;
}

void DemoGraphicsD3D11::DisposeTexture(uint64_t handle) noexcept {
    
    uintptr_t ptrValue = static_cast<uintptr_t>(handle);
    ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(ptrValue);
    
    if (srv) {
        
        srv->Release();
    }
}

CharIS::CODE DemoGraphicsD3D11::CreateAltasTexture(const CharIS::CreateTextureParam param, uint64_t& handle) noexcept {
    if (!initialized_ || !device_) {
        return CharIS::CODE_FAILED;
    }

    if (param.side == 0) {
        return CharIS::CODE_INVALIDARG;
    }

    // 确定纹理格式
    DXGI_FORMAT format = DXGI_FORMAT_R8_UNORM; // 默认 R8
    uint32_t bytesPerPixel = 1;
    
    if (param.channel == CharIS::TEXTURE_CHANNEL_R8G8B8) {
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bytesPerPixel = 4;
    }

    // 创建纹理描述
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = param.side;
    textureDesc.Height = param.side;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0; // DEFAULT 资源不支持 CPU 访问

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device_->CreateTexture2D(&textureDesc, nullptr, &texture);
    if (FAILED(hr)) {
        return CharIS::CODE_FAILED;
    }

    // 创建着色器资源视图
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device_->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr)) {
        texture->Release();
        return CharIS::CODE_FAILED;
    }

    // 将 SRV 指针转换为 uint64_t handle（兼容32位，先转换成 uintptr_t）
    uintptr_t ptrValue = reinterpret_cast<uintptr_t>(srv);
    handle = static_cast<uint64_t>(ptrValue);
    
    // texture 对象不需要单独存储，可以通过 SRV 获取
    texture->Release(); // SRV 会持有 texture 的引用，这里释放我们的引用

    return CharIS::CODE_OK;
}

void DemoGraphicsD3D11::DrawAltas(const DrawTextureParam& draw, const TextEffect& effect, void* context) noexcept
{
    (void)context;
    switch (draw.type)
    {
    case CharIS::TEXT_RENDER_TYPE_GRAY:
        this->DrawGray(draw, effect);
        break;
    case CharIS::TEXT_RENDER_TYPE_SUBPIXEL:
        this->DrawSubpixel(draw, effect);
        break;
    case CharIS::TEXT_RENDER_TYPE_MSDF:
        this->DrawMsdf(draw, effect);
        break;
    }
}

void DemoGraphicsD3D11::Upload(uint64_t handle, uint64_t id, CharIS::Box2D box, const void* data, uint32_t pitch) noexcept
{
    if (!context_upload_ || !data) return;

    // test sleep
    //::Sleep(1);

    uintptr_t ptrValue = static_cast<uintptr_t>(handle);
    ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(ptrValue);
    if (!srv) return;

    ID3D11Resource* resource = nullptr;
    srv->GetResource(&resource);
    if (!resource) return;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&texture));
    resource->Release();
    if (FAILED(hr) || !texture) return;

    D3D11_BOX destBox = {};
    destBox.left = box.x;
    destBox.top = box.y;
    destBox.right = box.x + box.width;
    destBox.bottom = box.y + box.height;
    destBox.front = 0;
    destBox.back = 1;

    upload_mtx_.lock();
    tasks_.push_back(id);
    // 使用 UpdateSubresource1 以避免延迟上下文上的非零偏移警告
    context_upload_->UpdateSubresource1(texture, 0, &destBox, data, pitch, 0, D3D11_COPY_NO_OVERWRITE);
    upload_mtx_.unlock();
    texture->Release();
}

void DemoGraphicsD3D11::FillRect(uint32_t background, void* context, fppoint_t point, fpsize_t size) noexcept
{
    (void)context;
    constexpr float fp26dot6_scale = 1.0f / 64.0f;
    float x = static_cast<float>(point.x) * fp26dot6_scale;
    float y = static_cast<float>(point.y) * fp26dot6_scale;
    float width = static_cast<float>(size.width) * fp26dot6_scale;
    float height = static_cast<float>(size.height) * fp26dot6_scale;
    FillRect(x, y, width, height, background);
}

void DemoGraphicsD3D11::DrawGray(const DrawTextureParam& draw, const TextEffect& effect)
{
    if (!batchRenderingInitialized_ || !context_) {
        return;
    }
    assert(grayCurrentTextureHandle_ == 0 || grayCurrentTextureHandle_ == draw.handle);
    grayCurrentTextureHandle_ = draw.handle;

    // 如果 batch 已满，需要 flush
    if (grayCurrentBatchCount_ >= TEXT_BATCH_COUNT) {
        Flush();
    }
    
    uint32_t color = effect.foreground;

    GrayInstanceData inst = {};
    inst.dstX = draw.dst.point.x;
    inst.dstY = draw.dst.point.y;
    inst.dstW = draw.dst.size.width;
    inst.dstH = draw.dst.size.height;
    inst.srcX = static_cast<uint16_t>(draw.src.x);
    inst.srcY = static_cast<uint16_t>(draw.src.y);
    inst.srcW = static_cast<uint16_t>(draw.src.width);
    inst.srcH = static_cast<uint16_t>(draw.src.height);
    inst.color = color;

    grayInstanceData_.push_back(inst);
    grayCurrentBatchCount_++;
}

void DemoGraphicsD3D11::DrawSubpixel(const DrawTextureParam& draw, const TextEffect& effect)
{
    if (!batchRenderingInitialized_ || !context_) {
        return;
    }
    assert(subpixelCurrentTextureHandle_ == 0 || subpixelCurrentTextureHandle_ == draw.handle);
    subpixelCurrentTextureHandle_ = draw.handle;

    if (subpixelCurrentBatchCount_ >= TEXT_BATCH_COUNT) {
        Flush();
    }

    uint32_t color = effect.foreground;

    GrayInstanceData inst = {};
    inst.dstX = draw.dst.point.x;
    inst.dstY = draw.dst.point.y;
    inst.dstW = draw.dst.size.width;
    inst.dstH = draw.dst.size.height;
    inst.srcX = static_cast<uint16_t>(draw.src.x);
    inst.srcY = static_cast<uint16_t>(draw.src.y);
    inst.srcW = static_cast<uint16_t>(draw.src.width);
    inst.srcH = static_cast<uint16_t>(draw.src.height);
    inst.color = color;

    subpixelInstanceData_.push_back(inst);
    subpixelCurrentBatchCount_++;
}

void DemoGraphicsD3D11::DrawMsdf(const DrawTextureParam& draw, const TextEffect& effect)
{
    if (!batchRenderingInitialized_ || !context_) {
        return;
    }

    // MSDF 批次必须使用同一张纹理
    assert(msdfCurrentTextureHandle_ == 0 || msdfCurrentTextureHandle_ == draw.handle);
    msdfCurrentTextureHandle_ = draw.handle;

    // 如果 batch 已满，需要 flush
    if (msdfCurrentBatchCount_ >= TEXT_BATCH_COUNT) {
        Flush();
    }

    uint32_t color1 = effect.foreground;
    uint32_t color2 = effect.outline;
                           // font-size  fp26.6   base-range
    //const float range = float(draw.size) / 64.f * 4.f;

    MsdfInstanceData inst = {};
    inst.dstX = draw.dst.point.x;
    inst.dstY = draw.dst.point.y;
    inst.dstW = draw.dst.size.width;
    inst.dstH = draw.dst.size.height;
    inst.srcX = static_cast<uint16_t>(draw.src.x);
    inst.srcY = static_cast<uint16_t>(draw.src.y);
    inst.srcW = static_cast<uint16_t>(draw.src.width);
    inst.srcH = static_cast<uint16_t>(draw.src.height);
    inst.color1 = color1;
    inst.color2 = color2;
    inst.size = draw.size;

    msdfInstanceData_.push_back(inst);
    msdfCurrentBatchCount_++;
}

void DemoGraphicsD3D11::FillRect(float x, float y, float width, float height, uint32_t color)
{
    if (!batchRenderingInitialized_ || !context_) {
        return;
    }
    
    if (rectFillCurrentBatchCount_ >= TEXT_BATCH_COUNT) {
        FlushRect();
    }
    
    RectInstanceData inst = {};
    inst.rect = DirectX::XMFLOAT4(x, y, width, height);
    inst.color = color;
    rectInstanceData_.push_back(inst);
    rectFillCurrentBatchCount_++;
}

void DemoGraphicsD3D11::Flush() noexcept
{
    FlushRect();
    FlushGray();
    FlushSubpixel();
    FlushMsdf();
}

void DemoGraphicsD3D11::FlushGray() noexcept
{
    if (!batchRenderingInitialized_ || !context_ || grayCurrentBatchCount_ == 0) {
        return;
    }
    if (grayInstanceData_.empty()) {
        grayCurrentBatchCount_ = 0;
        return;
    }

    // 上传实例数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context_->Map(grayInstanceBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, grayInstanceData_.data(), grayInstanceData_.size() * sizeof(GrayInstanceData));
        context_->Unmap(grayInstanceBuffer_, 0);
    } 
    else {
        D3D11_BOX box = {};
        box.right = static_cast<UINT>(grayInstanceData_.size() * sizeof(GrayInstanceData));
        box.bottom = 1;
        box.back = 1;
        context_->UpdateSubresource(grayInstanceBuffer_, 0, &box,
            grayInstanceData_.data(), sizeof(GrayInstanceData), 0);
    }

    context_->IASetInputLayout(grayLayout_);

    UINT strides[2] = { sizeof(GrayQuadVertex), sizeof(GrayInstanceData) };
    UINT offsets[2] = { 0, 0 };
    ID3D11Buffer* vbs[2] = { grayQuadVertexBuffer_, grayInstanceBuffer_ };
    context_->IASetVertexBuffers(0, 2, vbs, strides, offsets);
    context_->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context_->VSSetShader(grayVs_, nullptr, 0);
    context_->PSSetShader(grayPs_, nullptr, 0);

    uintptr_t ptrValue = static_cast<uintptr_t>(grayCurrentTextureHandle_);
    ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(ptrValue);
    if (srv) {
        ID3D11ShaderResourceView* srvs[] = { srv };
        context_->PSSetShaderResources(0, 1, srvs);
    }
    if (samplerStatePoint_) {
        ID3D11SamplerState* samplers[] = { samplerStatePoint_ };
        context_->PSSetSamplers(0, 1, samplers);
    }
    if (constantBuffer_) {
        context_->VSSetConstantBuffers(0, 1, &constantBuffer_);
        context_->PSSetConstantBuffers(0, 1, &constantBuffer_);
    }
    if (renderTargetView_)
        context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    context_->RSSetViewports(1, &viewport_);
    if (rasterizerState_)
        context_->RSSetState(rasterizerState_);
    if (blendState_) {
        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context_->OMSetBlendState(blendState_, blendFactor, 0xFFFFFFFFu);
    }

    context_->DrawInstanced(4, grayCurrentBatchCount_, 0, 0);

    grayInstanceData_.clear();
    grayCurrentBatchCount_ = 0;
    grayCurrentTextureHandle_ = 0;
}

void DemoGraphicsD3D11::FlushSubpixel() noexcept
{
    if (!batchRenderingInitialized_ || !context_ || subpixelCurrentBatchCount_ == 0) {
        return;
    }
    if (subpixelInstanceData_.empty()) {
        subpixelCurrentBatchCount_ = 0;
        subpixelCurrentTextureHandle_ = 0;
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context_->Map(subpixelInstanceBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, subpixelInstanceData_.data(), subpixelInstanceData_.size() * sizeof(GrayInstanceData));
        context_->Unmap(subpixelInstanceBuffer_, 0);
    }
    else {
        D3D11_BOX box = {};
        box.right = static_cast<UINT>(subpixelInstanceData_.size() * sizeof(GrayInstanceData));
        box.bottom = 1;
        box.back = 1;
        context_->UpdateSubresource(subpixelInstanceBuffer_, 0, &box,
            subpixelInstanceData_.data(), sizeof(GrayInstanceData), 0);
    }

    context_->IASetInputLayout(grayLayout_);

    UINT strides[2] = { sizeof(GrayQuadVertex), sizeof(GrayInstanceData) };
    UINT offsets[2] = { 0, 0 };
    ID3D11Buffer* vbs[2] = { grayQuadVertexBuffer_, subpixelInstanceBuffer_ };
    context_->IASetVertexBuffers(0, 2, vbs, strides, offsets);
    context_->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context_->VSSetShader(grayVs_, nullptr, 0);
    context_->PSSetShader(subpixelPs_, nullptr, 0);

    uintptr_t ptrValue = static_cast<uintptr_t>(subpixelCurrentTextureHandle_);
    ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(ptrValue);
    if (srv) {
        ID3D11ShaderResourceView* srvs[] = { srv };
        context_->PSSetShaderResources(0, 1, srvs);
    }
    if (samplerStatePoint_) {
        ID3D11SamplerState* samplers[] = { samplerStatePoint_ };
        context_->PSSetSamplers(0, 1, samplers);
    }
    if (constantBuffer_) {
        context_->VSSetConstantBuffers(0, 1, &constantBuffer_);
        context_->PSSetConstantBuffers(0, 1, &constantBuffer_);
    }
    if (renderTargetView_)
        context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    context_->RSSetViewports(1, &viewport_);
    if (rasterizerState_)
        context_->RSSetState(rasterizerState_);
    if (blendStateSubpixel_) {
        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context_->OMSetBlendState(blendStateSubpixel_, blendFactor, 0xFFFFFFFFu);
    }

    context_->DrawInstanced(4, subpixelCurrentBatchCount_, 0, 0);

    subpixelInstanceData_.clear();
    subpixelCurrentBatchCount_ = 0;
    subpixelCurrentTextureHandle_ = 0;
}

void DemoGraphicsD3D11::FlushMsdf() noexcept
{
    if (!batchRenderingInitialized_ || !context_ || msdfCurrentBatchCount_ == 0) {
        return;
    }
    if (msdfInstanceData_.empty()) {
        msdfCurrentBatchCount_ = 0;
        msdfCurrentTextureHandle_ = 0;
        return;
    }

    // 上传 MSDF 实例数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context_->Map(msdfInstanceBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, msdfInstanceData_.data(), msdfInstanceData_.size() * sizeof(MsdfInstanceData));
        context_->Unmap(msdfInstanceBuffer_, 0);
    }
    else {
        D3D11_BOX box = {};
        box.right = static_cast<UINT>(msdfInstanceData_.size() * sizeof(MsdfInstanceData));
        box.bottom = 1;
        box.back = 1;
        context_->UpdateSubresource(msdfInstanceBuffer_, 0, &box,
            msdfInstanceData_.data(), sizeof(MsdfInstanceData), 0);
    }

    context_->IASetInputLayout(msdfLayout_);

    UINT strides[2] = { sizeof(GrayQuadVertex), sizeof(MsdfInstanceData) };
    UINT offsets[2] = { 0, 0 };
    ID3D11Buffer* vbs[2] = { grayQuadVertexBuffer_, msdfInstanceBuffer_ };
    context_->IASetVertexBuffers(0, 2, vbs, strides, offsets);
    context_->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context_->VSSetShader(msdfInstancedVs_, nullptr, 0);
    context_->PSSetShader(msdfPs_, nullptr, 0);

    uintptr_t ptrValue = static_cast<uintptr_t>(msdfCurrentTextureHandle_);
    ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(ptrValue);
    if (srv) {
        ID3D11ShaderResourceView* srvs[] = { srv };
        context_->PSSetShaderResources(0, 1, srvs);
    }
    if (samplerStateLinear_) {
        ID3D11SamplerState* samplers[] = { samplerStateLinear_ };
        context_->PSSetSamplers(0, 1, samplers);
    }
    if (constantBuffer_) {
        context_->VSSetConstantBuffers(0, 1, &constantBuffer_);
        context_->PSSetConstantBuffers(0, 1, &constantBuffer_);
    }
    if (renderTargetView_)
        context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    context_->RSSetViewports(1, &viewport_);
    if (rasterizerState_)
        context_->RSSetState(rasterizerState_);
    if (blendState_) {
        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context_->OMSetBlendState(blendState_, blendFactor, 0xFFFFFFFFu);
    }

    context_->DrawInstanced(4, msdfCurrentBatchCount_, 0, 0);

    msdfInstanceData_.clear();
    msdfCurrentBatchCount_ = 0;
    msdfCurrentTextureHandle_ = 0;
}

void DemoGraphicsD3D11::FlushRect() noexcept
{
    if (!batchRenderingInitialized_ || !context_ || rectFillCurrentBatchCount_ == 0) {
        return;
    }
    if (rectInstanceData_.empty()) {
        rectFillCurrentBatchCount_ = 0;
        return;
    }

    // 上传实例数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context_->Map(rectInstanceBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, rectInstanceData_.data(), rectInstanceData_.size() * sizeof(RectInstanceData));
        context_->Unmap(rectInstanceBuffer_, 0);
    } else {
        D3D11_BOX box = {};
        box.right = static_cast<UINT>(rectInstanceData_.size() * sizeof(RectInstanceData));
        box.bottom = 1;
        box.back = 1;
        context_->UpdateSubresource(rectInstanceBuffer_, 0, &box,
            rectInstanceData_.data(), sizeof(RectInstanceData), 0);
    }

    context_->IASetInputLayout(rectFillInstancedLayout_);
    UINT strides[2] = { sizeof(RectQuadVertex), sizeof(RectInstanceData) };
    UINT offsets[2] = { 0, 0 };
    ID3D11Buffer* vbs[2] = { rectQuadVertexBuffer_, rectInstanceBuffer_ };
    context_->IASetVertexBuffers(0, 2, vbs, strides, offsets);
    context_->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context_->VSSetShader(rectFillInstancedVs_, nullptr, 0);
    context_->PSSetShader(rectFillPs_, nullptr, 0);

    if (constantBuffer_) {
        context_->VSSetConstantBuffers(0, 1, &constantBuffer_);
        context_->PSSetConstantBuffers(0, 1, &constantBuffer_);
    }
    if (renderTargetView_)
        context_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    context_->RSSetViewports(1, &viewport_);
    if (rasterizerState_)
        context_->RSSetState(rasterizerState_);
    if (blendState_) {
        const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context_->OMSetBlendState(blendState_, blendFactor, 0xFFFFFFFFu);
    }

    context_->DrawInstanced(4, rectFillCurrentBatchCount_, 0, 0);

    rectInstanceData_.clear();
    rectFillCurrentBatchCount_ = 0;
}

void DemoGraphicsD3D11::Release() noexcept {
    Shutdown();
    delete this;
}

CODE DemoGraphicsD3D11::InitializeBatchRendering() noexcept {
    if (batchRenderingInitialized_  || !device_) {
        return CharIS::CODE_FAILED;
    }
    
    HRESULT hr = S_OK;
    
    // 读取 shader 文件
    FILE* file = nullptr;
    if (fopen_s(&file, "shaders.hlsl", "rb") != 0 || !file) {
        return CharIS::CODE_FAILED;
    }
    
    std::vector<char> buf;
    fseek(file, 0, SEEK_END);
    const auto len = ftell(file);
    buf.resize(len + 1);
    fseek(file, 0, SEEK_SET);
    fread(buf.data(), 1, len, file);
    buf[len] = '\0';
    fclose(file);
    
    const char* s_demo_shader = buf.data();

    constexpr float initRange = 1.f / 32.f;
    
    // 矩形的4(6)个顶点 与相应纹理坐标
    const MsdfVertex vertices[] = {
        { DirectX::XMFLOAT2(0, 0), DirectX::XMFLOAT4(0.f, 1.f, 0.f, initRange), 0xff0000ff, 0 },
        { DirectX::XMFLOAT2(1, 0), DirectX::XMFLOAT4(1.f, 1.f, 0.f, initRange), 0xffffffff, 0 },
        { DirectX::XMFLOAT2(1, 1), DirectX::XMFLOAT4(1.f, 0.f, 0.f, initRange), 0xffffffff, 0 },
        { DirectX::XMFLOAT2(0, 0), DirectX::XMFLOAT4(0.f, 1.f, 0.f, initRange), 0xff0000ff, 0 },
        { DirectX::XMFLOAT2(1, 1), DirectX::XMFLOAT4(1.f, 0.f, 0.f, initRange), 0xffffffff, 0 },
        { DirectX::XMFLOAT2(0, 1), DirectX::XMFLOAT4(0.f, 0.f, 0.f, initRange), 0xff0000ff, 0 },
    };
    
    // 输入布局
    const D3D11_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(MsdfVertex, pos),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, offsetof(MsdfVertex, tex),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UINT,       0, offsetof(MsdfVertex, color1), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    1, DXGI_FORMAT_R8G8B8A8_UINT,       0, offsetof(MsdfVertex, color2), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    ID3DBlob* vs = nullptr;
    ID3DBlob* vs2 = nullptr;
    ID3DBlob* vsGray = nullptr;
    ID3DBlob* vsRectFill = nullptr;
    ID3DBlob* vsRectFillInstanced = nullptr;
    ID3DBlob* vsMsdfInstanced = nullptr;
    ID3DBlob* vsp = nullptr;
    ID3DBlob* ps_msdf = nullptr;
    ID3DBlob* ps2 = nullptr;
    ID3DBlob* ps3 = nullptr;
    ID3DBlob* ps4 = nullptr;
    ID3DBlob* psSubpixel = nullptr;
    ID3DBlob* psRectFill = nullptr;
    ID3DBlob* psp = nullptr;
    ID3DBlob* gs = nullptr;
    
    // 灰度实例化：单位四边形顶点缓冲（4 顶点三角带：左下→左上→右下→右上，无索引）
    if (SUCCEEDED(hr)) {
        GrayQuadVertex quadVerts[4] = {
            { DirectX::XMFLOAT2(0.f, 1.f), DirectX::XMFLOAT2(0.f, 1.f) }, // 左下
            { DirectX::XMFLOAT2(0.f, 0.f), DirectX::XMFLOAT2(0.f, 0.f) }, // 左上
            { DirectX::XMFLOAT2(1.f, 1.f), DirectX::XMFLOAT2(1.f, 1.f) }, // 右下
            { DirectX::XMFLOAT2(1.f, 0.f), DirectX::XMFLOAT2(1.f, 0.f) }, // 右上
        };
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.ByteWidth = sizeof(quadVerts);
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sub_data = {};
        sub_data.pSysMem = quadVerts;
        hr = device_->CreateBuffer(&buffer_desc, &sub_data, &grayQuadVertexBuffer_);
    }
    // 灰度实例化：实例数据缓冲
    if (SUCCEEDED(hr)) {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(GrayInstanceData) * TEXT_BATCH_COUNT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device_->CreateBuffer(&buffer_desc, nullptr, &grayInstanceBuffer_);
    }
    // Subpixel 实例化：实例数据缓冲（与 Gray 同布局）
    if (SUCCEEDED(hr)) {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(GrayInstanceData) * TEXT_BATCH_COUNT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device_->CreateBuffer(&buffer_desc, nullptr, &subpixelInstanceBuffer_);
    }
    // MSDF 实例化：实例数据缓冲（color1/color2/range）
    if (SUCCEEDED(hr)) {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(MsdfInstanceData) * TEXT_BATCH_COUNT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device_->CreateBuffer(&buffer_desc, nullptr, &msdfInstanceBuffer_);
    }
    
    // 纯色矩形实例化：单位四边形顶点缓冲（4 顶点三角带，与灰度一致）
    if (SUCCEEDED(hr)) {
        RectQuadVertex quadVerts[4] = {
            { DirectX::XMFLOAT2(0.f, 1.f) }, // 左下
            { DirectX::XMFLOAT2(0.f, 0.f) }, // 左上
            { DirectX::XMFLOAT2(1.f, 1.f) }, // 右下
            { DirectX::XMFLOAT2(1.f, 0.f) }, // 右上
        };
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.ByteWidth = sizeof(quadVerts);
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sub_data = {};
        sub_data.pSysMem = quadVerts;
        hr = device_->CreateBuffer(&buffer_desc, &sub_data, &rectQuadVertexBuffer_);
    }
    // 纯色矩形实例化：实例数据缓冲
    if (SUCCEEDED(hr)) {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(RectInstanceData) * TEXT_BATCH_COUNT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device_->CreateBuffer(&buffer_desc, nullptr, &rectInstanceBuffer_);
    }
    
    // 编译标志
    UINT compileFlags = 0;

#ifndef NDEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* errorMsg = nullptr;

    // 编译 VS - vs_main (unused)
    //if (SUCCEEDED(hr)) {
    //
    //    hr = D3DCompile(
    //        s_demo_shader, len,
    //        nullptr, nullptr, nullptr,
    //        "vs_main", "vs_4_0",
    //        compileFlags, 0, &vs, nullptr //&errorMsg
    //    );
    //    //if (errorMsg) {
    //    //    const auto msg = errorMsg->GetBufferPointer();
    //    //    const auto len = errorMsg->GetBufferSize();
    //    //    ::OutputDebugStringA((char*)errorMsg);
    //    //    errorMsg->Release();
    //    //}
    //}
    //
    //// 编译 VS - vs_pixel (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = D3DCompile(
    //        s_demo_shader, len,
    //        nullptr, nullptr, nullptr,
    //        "vs_pixel", "vs_4_0",
    //        compileFlags, 0, &vs2, nullptr
    //    );
    //}
    
    // 编译 VS - vs_gray
    if (SUCCEEDED(hr)) {
        hr = D3DCompile(
            s_demo_shader, len,
            nullptr, nullptr, nullptr,
            "vs_gray", "vs_4_0",
            compileFlags, 0, &vsGray, nullptr
        );
    }
    
    // 编译 VS - vs_rect_fill (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = D3DCompile(
    //        s_demo_shader, len,
    //        nullptr, nullptr, nullptr,
    //        "vs_rect_fill", "vs_4_0",
    //        compileFlags, 0, &vsRectFill, nullptr
    //    );
    //}
    // 编译 VS - vs_rect_fill_instanced（纯色矩形实例化）
    if (SUCCEEDED(hr)) {
        hr = D3DCompile(
            s_demo_shader, len,
            nullptr, nullptr, nullptr,
            "vs_rect_fill_instanced", "vs_4_0",
            compileFlags, 0, &vsRectFillInstanced, nullptr
        );
    }
    // 编译 VS - vs_msdf_instanced（MSDF 实例化）
    if (SUCCEEDED(hr)) {
        hr = D3DCompile(
            s_demo_shader, len,
            nullptr, nullptr, nullptr,
            "vs_msdf_instanced", "vs_4_0",
            compileFlags, 0, &vsMsdfInstanced, nullptr
        );
    }
    
    // 编译 VS - vs_polygon (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = D3DCompile(
    //        s_demo_shader, len,
    //        nullptr, nullptr, nullptr,
    //        "vs_polygon", "vs_4_0",
    //        compileFlags, 0, &vsp, nullptr
    //    );
    //}
    
    // 编译 PS - ps_msdf
    if (SUCCEEDED(hr)) {
        hr = D3DCompile(
            s_demo_shader, len,
            nullptr, nullptr, nullptr,
            "ps_msdf", "ps_4_0",
            compileFlags, 0, &ps_msdf, nullptr
        );
    }
    
    // 编译 PS - ps_color (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = D3DCompile(
    //        s_demo_shader, len,
    //        nullptr, nullptr, nullptr,
    //        "ps_color", "ps_4_0",
    //        compileFlags, 0, &ps2, nullptr
    //    );
    //}
    
    // 编译 PS - ps_gray
    if (SUCCEEDED(hr)) {
        hr = D3DCompile(
            s_demo_shader, len,
            nullptr, nullptr, nullptr,
            "ps_gray", "ps_4_0",
            compileFlags, 0, &ps3, nullptr
        );
    }
    
    // 编译 PS - ps_rect_fill
    if (SUCCEEDED(hr)) {
        hr = D3DCompile(
            s_demo_shader, len,
            nullptr, nullptr, nullptr,
            "ps_rect_fill", "ps_4_0",
            compileFlags, 0, &psRectFill, nullptr
        );
    }
    
    // 编译 PS - ps_subh（子像素渲染，双输出用于 RGB 三通道 blend）
    if (SUCCEEDED(hr)) {
        hr = D3DCompile(
            s_demo_shader, len,
            nullptr, nullptr, nullptr,
            "ps_subh", "ps_4_0",
            compileFlags, 0, &psSubpixel, nullptr
        );
    }
    
    // 编译 PS - ps_polygon (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = D3DCompile(
    //        s_demo_shader, len,
    //        nullptr, nullptr, nullptr,
    //        "ps_polygon", "ps_4_0",
    //        compileFlags, 0, &psp, nullptr
    //    );
    //}
    
    // 编译 GS - gs_extrude (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = D3DCompile(
    //        s_demo_shader, len,
    //        nullptr, nullptr, nullptr,
    //        "gs_extrude", "gs_4_0",
    //        compileFlags, 0, &gs, nullptr
    //    );
    //}
    
    // 创建 Vertex Shader - vs_main (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreateVertexShader(
    //        vs->GetBufferPointer(),
    //        vs->GetBufferSize(),
    //        nullptr, &rectVs_
    //    );
    //}
    //
    //// 创建 Vertex Shader - vs_pixel (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreateVertexShader(
    //        vs2->GetBufferPointer(),
    //        vs2->GetBufferSize(),
    //        nullptr, &rectVsPixel_
    //    );
    //}
    
    // 创建 Vertex Shader - vs_gray
    if (SUCCEEDED(hr)) {
        hr = device_->CreateVertexShader(
            vsGray->GetBufferPointer(),
            vsGray->GetBufferSize(),
            nullptr, &grayVs_
        );
    }
    
    // 创建 Vertex Shader - vs_rect_fill (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreateVertexShader(
    //        vsRectFill->GetBufferPointer(),
    //        vsRectFill->GetBufferSize(),
    //        nullptr, &rectFillVs_
    //    );
    //}
    // 创建 Vertex Shader - vs_rect_fill_instanced
    if (SUCCEEDED(hr)) {
        hr = device_->CreateVertexShader(
            vsRectFillInstanced->GetBufferPointer(),
            vsRectFillInstanced->GetBufferSize(),
            nullptr, &rectFillInstancedVs_
        );
    }
    // 创建 Vertex Shader - vs_msdf_instanced
    if (SUCCEEDED(hr)) {
        hr = device_->CreateVertexShader(
            vsMsdfInstanced->GetBufferPointer(),
            vsMsdfInstanced->GetBufferSize(),
            nullptr, &msdfInstancedVs_
        );
    }
    
    // 创建 Pixel Shader - ps_main
    if (SUCCEEDED(hr)) {
        hr = device_->CreatePixelShader(
            ps_msdf->GetBufferPointer(),
            ps_msdf->GetBufferSize(),
            nullptr, &msdfPs_
        );
    }
    
    // 创建 Pixel Shader - ps_gray
    if (SUCCEEDED(hr)) {
        hr = device_->CreatePixelShader(
            ps3->GetBufferPointer(),
            ps3->GetBufferSize(),
            nullptr, &grayPs_
        );
    }
    
    // 创建 Pixel Shader - ps_rect_fill
    if (SUCCEEDED(hr)) {
        hr = device_->CreatePixelShader(
            psRectFill->GetBufferPointer(),
            psRectFill->GetBufferSize(),
            nullptr, &rectFillPs_
        );
    }
    
    // 创建 Pixel Shader - ps_subh（子像素 dual-source blend）
    if (SUCCEEDED(hr)) {
        hr = device_->CreatePixelShader(
            psSubpixel->GetBufferPointer(),
            psSubpixel->GetBufferSize(),
            nullptr, &subpixelPs_
        );
    }
    //
    //// 创建 Pixel Shader - ps_color (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreatePixelShader(
    //        ps2->GetBufferPointer(),
    //        ps2->GetBufferSize(),
    //        nullptr, &colorPs_
    //    );
    //}
    
    // 创建 Vertex Shader - vs_polygon (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreateVertexShader(
    //        vsp->GetBufferPointer(),
    //        vsp->GetBufferSize(),
    //        nullptr, &polygonVs_
    //    );
    //}
    //
    //// 创建 Pixel Shader - ps_polygon (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreatePixelShader(
    //        psp->GetBufferPointer(),
    //        psp->GetBufferSize(),
    //        nullptr, &polygonPs_
    //    );
    //}
    //
    //// 创建 Geometry Shader - gs_extrude (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreateGeometryShader(
    //        gs->GetBufferPointer(),
    //        gs->GetBufferSize(),
    //        nullptr, &extrudeGs_
    //    );
    //}
    
    // 创建 Input Layout - Rect (unused)
    //if (SUCCEEDED(hr)) {
    //    hr = device_->CreateInputLayout(
    //        inputLayout, ARRAYSIZE(inputLayout),
    //        vs->GetBufferPointer(),
    //        vs->GetBufferSize(),
    //        &rectLayout_
    //    );
    //}
    
    // 创建 Input Layout - Gray 实例化（dst fp26.6 / src 像素 16bit，与 MSDF 一致）
    if (SUCCEEDED(hr)) {
        const D3D11_INPUT_ELEMENT_DESC grayInputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_SINT,  1, 0,                      D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "TEXCOORD", 2, DXGI_FORMAT_R16G16B16A16_UINT, 1, 16,                      D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "COLOR",    1, DXGI_FORMAT_R8G8B8A8_UINT, 1, 24,                           D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        hr = device_->CreateInputLayout(
            grayInputLayout, ARRAYSIZE(grayInputLayout),
            vsGray->GetBufferPointer(),
            vsGray->GetBufferSize(),
            &grayLayout_
        );
    }
    
    // 创建 Input Layout - Rect Fill (unused)
    //if (SUCCEEDED(hr)) {
    //    const D3D11_INPUT_ELEMENT_DESC rectFillInputLayout[] = {
    //        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(RectVertex, pos),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UINT,       0, offsetof(RectVertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //    };
    //    
    //    hr = device_->CreateInputLayout(
    //        rectFillInputLayout, ARRAYSIZE(rectFillInputLayout),
    //        vsRectFill->GetBufferPointer(),
    //        vsRectFill->GetBufferSize(),
    //        &rectFillLayout_
    //    );
    //}
    // 创建 Input Layout - Rect Fill 实例化（slot0 单位四边形，slot1 实例数据）
    if (SUCCEEDED(hr)) {
        const D3D11_INPUT_ELEMENT_DESC rectFillInstancedInputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,                       D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "COLOR",    1, DXGI_FORMAT_R8G8B8A8_UINT, 1, 16,                           D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        hr = device_->CreateInputLayout(
            rectFillInstancedInputLayout, ARRAYSIZE(rectFillInstancedInputLayout),
            vsRectFillInstanced->GetBufferPointer(),
            vsRectFillInstanced->GetBufferSize(),
            &rectFillInstancedLayout_
        );
    }
    // 创建 Input Layout - MSDF 实例化（dst fp26.6 / src 像素 16bit / color1/color2/range）
    if (SUCCEEDED(hr)) {
        const D3D11_INPUT_ELEMENT_DESC msdfInputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_SINT,  1, 0,                      D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "TEXCOORD", 2, DXGI_FORMAT_R16G16B16A16_UINT, 1, 16,                      D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UINT, 1, 24,                           D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "COLOR",    1, DXGI_FORMAT_R8G8B8A8_UINT, 1, 28,                           D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "TEXCOORD", 3, DXGI_FORMAT_R32_SINT, 1, 32,                               D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        hr = device_->CreateInputLayout(
            msdfInputLayout, ARRAYSIZE(msdfInputLayout),
            vsMsdfInstanced->GetBufferPointer(),
            vsMsdfInstanced->GetBufferSize(),
            &msdfLayout_
        );
    }
    
    // 创建 Input Layout - Polygon (unused)
    //if (SUCCEEDED(hr)) {
    //    const D3D11_INPUT_ELEMENT_DESC polygonInputLayout[] = {
    //        { "POSITION",     0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(PolygonVertex, pos),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //        { "NORMAL",       0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(PolygonVertex, nor),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //        { "BLENDINDICES", 0, DXGI_FORMAT_R32_UINT,     0, offsetof(PolygonVertex, index), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //    };
    //    
    //    hr = device_->CreateInputLayout(
    //        polygonInputLayout, ARRAYSIZE(polygonInputLayout),
    //        vsp->GetBufferPointer(),
    //        vsp->GetBufferSize(),
    //        &polygonLayout_
    //    );
    //}
    
    // 创建 Constant Buffer
    if (SUCCEEDED(hr)) {
        
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(AffineMatrix2D);
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        // 初始化为单位矩阵和默认屏幕尺寸
        AffineMatrix2D initData = {};
        initData.m12 = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);  // 单位矩阵
        initData.dxy = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        initData.screen = DirectX::XMFLOAT4(800.f * 2.f, 600.f * 2.f, 1.0f / 800.f / 2.f, 1.0f / 600.0f / 2.f);
        initData.texParams = DirectX::XMFLOAT4(2048.f, 2048.f, 1.0f / 2048.f, 1.0f / 2048.f);
        
        D3D11_SUBRESOURCE_DATA sub_data = {};
        sub_data.pSysMem = &initData;
        hr = device_->CreateBuffer(&buffer_desc, &sub_data, &constantBuffer_);
    }
    
    // 创建 Sampler State - Point（灰度和子像素）
    if (SUCCEEDED(hr)) {
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.BorderColor[0] = 0.0f;
        sampler_desc.BorderColor[1] = 0.0f;
        sampler_desc.BorderColor[2] = 0.0f;
        sampler_desc.BorderColor[3] = 0.0f;
        sampler_desc.MinLOD = 0.0f;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device_->CreateSamplerState(&sampler_desc, &samplerStatePoint_);
    }
    // 创建 Sampler State - Linear（MSDF）
    if (SUCCEEDED(hr)) {
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.BorderColor[0] = 0.0f;
        sampler_desc.BorderColor[1] = 0.0f;
        sampler_desc.BorderColor[2] = 0.0f;
        sampler_desc.BorderColor[3] = 0.0f;
        sampler_desc.MinLOD = 0.0f;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device_->CreateSamplerState(&sampler_desc, &samplerStateLinear_);
    }
    
    // 创建 Rasterizer State - 裁剪背面三角形
    if (SUCCEEDED(hr)) {
        D3D11_RASTERIZER_DESC rasterizer_desc = {};
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;  // 裁剪背面三角形
        rasterizer_desc.FrontCounterClockwise = FALSE;
        rasterizer_desc.DepthBias = 0;
        rasterizer_desc.DepthBiasClamp = 0.0f;
        rasterizer_desc.SlopeScaledDepthBias = 0.0f;
        rasterizer_desc.DepthClipEnable = TRUE;
        rasterizer_desc.ScissorEnable = FALSE;
        rasterizer_desc.MultisampleEnable = FALSE;
        rasterizer_desc.AntialiasedLineEnable = FALSE;
        
        hr = device_->CreateRasterizerState(&rasterizer_desc, &rasterizerState_);
    }
    
    // 创建 Blend State - 普通 Alpha 混合 (SrcAlpha, OneMinusSrcAlpha)
    if (SUCCEEDED(hr)) {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        auto& rt = blendDesc.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device_->CreateBlendState(&blendDesc, &blendState_);
    }
    // 创建 Blend State - 子像素 RGB 三通道 blend (dual-source: SRC1/INV_SRC1)
    if (SUCCEEDED(hr)) {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        auto& rt = blendDesc.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D11_BLEND_SRC1_COLOR;
        rt.DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
        rt.BlendOp = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_ZERO;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device_->CreateBlendState(&blendDesc, &blendStateSubpixel_);
    }
    
    // 释放临时资源
    SafeRelease(vs);
    SafeRelease(vs2);
    SafeRelease(vsGray);
    SafeRelease(vsRectFill);
    SafeRelease(vsRectFillInstanced);
    SafeRelease(vsMsdfInstanced);
    SafeRelease(vsp);
    SafeRelease(ps_msdf);
    SafeRelease(ps2);
    SafeRelease(ps3);
    SafeRelease(ps4);
    SafeRelease(psSubpixel);
    SafeRelease(psRectFill);
    SafeRelease(psp);
    SafeRelease(gs);
    
    if (SUCCEEDED(hr)) {
        batchRenderingInitialized_ = true;
        return CharIS::CODE_OK;
    } 
    else {
        assert(!"NOTIMPL");
        ShutdownBatchRendering();
        return CharIS::CODE_FAILED;
    }
}

void DemoGraphicsD3D11::ShutdownBatchRendering() noexcept {
    Flush();      // 确保所有 batch 都被渲染（含 Gray/MSDF/Rect）
    
    //SafeRelease(vertexBuffer_);
    //SafeRelease(batchVertexBuffer_);
    SafeRelease(grayQuadVertexBuffer_);
    SafeRelease(grayInstanceBuffer_);
    SafeRelease(subpixelInstanceBuffer_);
    SafeRelease(rectQuadVertexBuffer_);
    SafeRelease(rectInstanceBuffer_);
    SafeRelease(msdfInstanceBuffer_);
    //SafeRelease(indexBuffer_);
    
    //SafeRelease(rectVs_);
    //SafeRelease(rectVsPixel_);
    SafeRelease(grayVs_);
    //SafeRelease(rectFillVs_);
    SafeRelease(rectFillInstancedVs_);
    SafeRelease(msdfInstancedVs_);
    SafeRelease(msdfPs_);
    SafeRelease(grayPs_);
    SafeRelease(subpixelPs_);
    SafeRelease(rectFillPs_);
    //SafeRelease(subpixPs_);
    //SafeRelease(colorPs_);
    //SafeRelease(polygonVs_);
    //SafeRelease(polygonPs_);
    //SafeRelease(extrudeGs_);
    
    //SafeRelease(rectLayout_);
    SafeRelease(grayLayout_);
    //SafeRelease(rectFillLayout_);
    SafeRelease(rectFillInstancedLayout_);
    SafeRelease(msdfLayout_);
    //SafeRelease(polygonLayout_);
    
    SafeRelease(constantBuffer_);
    SafeRelease(samplerStatePoint_);
    SafeRelease(samplerStateLinear_);
    SafeRelease(rasterizerState_);
    SafeRelease(blendState_);
    SafeRelease(blendStateSubpixel_);
    
    //batchVertices_.clear();
    //currentBatchCount_ = 0;
    //currentTextureHandle_ = 0;
    
    grayInstanceData_.clear();
    grayCurrentBatchCount_ = 0;
    grayCurrentTextureHandle_ = 0;

    subpixelInstanceData_.clear();
    subpixelCurrentBatchCount_ = 0;
    subpixelCurrentTextureHandle_ = 0;

    rectInstanceData_.clear();
    rectFillCurrentBatchCount_ = 0;

    msdfInstanceData_.clear();
    msdfCurrentBatchCount_ = 0;
    msdfCurrentTextureHandle_ = 0;
    
    batchRenderingInitialized_ = false;
}

extern "C" IDemoGraphics* CreateDemoGraphicsD3D11() {
    return new(std::nothrow) DemoGraphicsD3D11();
}



