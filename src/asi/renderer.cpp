#include "asi/renderer.h"

#include "asi/api.h"
#include "common/log.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace asterui::renderer {

namespace {

using Microsoft::WRL::ComPtr;

constexpr char kShaderSource[] = R"(
cbuffer Params : register(b0) {
    float4 rect;
    float opacity;
    float3 padding;
};
Texture2D frameTexture : register(t0);
SamplerState frameSampler : register(s0);

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vertexId : SV_VertexID) {
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    VSOut output;
    output.position = float4(
        lerp(rect.x, rect.z, uv.x),
        lerp(rect.y, rect.w, uv.y),
        0.0f,
        1.0f);
    output.uv = uv;
    return output;
}

float4 PSMain(VSOut input) : SV_Target {
    return frameTexture.Sample(frameSampler, input.uv) * opacity;
}
)";

struct alignas(16) ShaderParams {
    float rect[4];
    float opacity;
    float padding[3];
};

struct TextureState {
    uint32_t width = 0;
    uint32_t height = 0;
    LONG sequence = -1;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
};

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;
ComPtr<ID3D11SamplerState> g_sampler;
ComPtr<ID3D11BlendState> g_blend;
ComPtr<ID3D11RasterizerState> g_rasterizer;
ComPtr<ID3D11Buffer> g_params;
std::unordered_map<uint32_t, TextureState> g_textures;
bool g_failed = false;
bool g_ignoredSwapChainLogged = false;

bool IsGameSwapChain(IDXGISwapChain* swapChain) {
    if (!swapChain) {
        return false;
    }
    DXGI_SWAP_CHAIN_DESC description = {};
    if (FAILED(swapChain->GetDesc(&description)) || !description.OutputWindow) {
        return false;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(description.OutputWindow, &processId);
    if (processId != GetCurrentProcessId()) {
        return false;
    }
    wchar_t className[128] = {};
    if (GetClassNameW(
            description.OutputWindow,
            className,
            static_cast<int>(std::size(className))) != 0 &&
        (wcsstr(className, L"TEMPWINDOW") || wcsstr(className, L"RGSCD3D"))) {
        if (!g_ignoredSwapChainLogged) {
            g_ignoredSwapChainLogged = true;
            Log::Info("ignoring auxiliary Rockstar swap chain");
        }
        return false;
    }
    return true;
}

bool EnsureDevice(IDXGISwapChain* swapChain) {
    if (!IsGameSwapChain(swapChain)) {
        return false;
    }
    ComPtr<ID3D11Device> current;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), &current))) {
        if (!g_failed) {
            Log::Error("swap chain is not an ID3D11 device");
            g_failed = true;
        }
        return false;
    }
    if (g_device && g_context) {
        return current.Get() == g_device.Get();
    }
    current->GetImmediateContext(&g_context);
    g_device = current;
    return true;
}

bool CompileShaders() {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    ComPtr<ID3DBlob> vertexBlob;
    ComPtr<ID3DBlob> pixelBlob;
    ComPtr<ID3DBlob> errors;
    if (FAILED(D3DCompile(
            kShaderSource,
            sizeof(kShaderSource) - 1,
            "AsterUI.hlsl",
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_0",
            flags,
            0,
            &vertexBlob,
            &errors)) ||
        FAILED(D3DCompile(
            kShaderSource,
            sizeof(kShaderSource) - 1,
            "AsterUI.hlsl",
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_0",
            flags,
            0,
            &pixelBlob,
            &errors))) {
        Log::Error("AsterUI shader compilation failed");
        return false;
    }
    return SUCCEEDED(g_device->CreateVertexShader(
               vertexBlob->GetBufferPointer(),
               vertexBlob->GetBufferSize(),
               nullptr,
               &g_vertexShader)) &&
        SUCCEEDED(g_device->CreatePixelShader(
               pixelBlob->GetBufferPointer(),
               pixelBlob->GetBufferSize(),
               nullptr,
               &g_pixelShader));
}

bool CreatePipeline() {
    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(g_device->CreateSamplerState(&sampler, &g_sampler))) {
        return false;
    }

    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(g_device->CreateBlendState(&blend, &g_blend))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizer = {};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    if (FAILED(g_device->CreateRasterizerState(&rasterizer, &g_rasterizer))) {
        return false;
    }

    D3D11_BUFFER_DESC buffer = {};
    buffer.ByteWidth = sizeof(ShaderParams);
    buffer.Usage = D3D11_USAGE_DYNAMIC;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_device->CreateBuffer(&buffer, nullptr, &g_params))) {
        return false;
    }
    return true;
}

bool EnsurePipeline() {
    if (g_vertexShader && g_pixelShader && g_sampler && g_blend &&
        g_rasterizer && g_params) {
        return true;
    }
    return CompileShaders() && CreatePipeline();
}

bool UploadFrame(
    TextureState& target,
    const std::shared_ptr<SharedFrame>& frame) {
    const auto keepLastFrame = [&target]() {
        return target.view.Get() != nullptr;
    };
    if (!frame || !frame->Header()) {
        return keepLastFrame();
    }
    FrameHeader* header = frame->Header();
    const LONG sequence = InterlockedCompareExchange(
        &header->publishedSequence,
        0,
        0);
    if (sequence == target.sequence || (target.sequence < 0 && sequence <= 0)) {
        return target.texture.Get() != nullptr;
    }

    const LONG publishedSlot = InterlockedCompareExchange(
        &header->publishedSlot,
        0,
        0);
    if (publishedSlot < 0 || publishedSlot >= static_cast<LONG>(kFrameSlots)) {
        return keepLastFrame();
    }
    const uint32_t slot = static_cast<uint32_t>(publishedSlot);
    const LONG before = header->slotSequence[slot];
    if ((before & 1) != 0) {
        return keepLastFrame();
    }
    const uint32_t width = header->width;
    const uint32_t height = header->height;
    const int stride = static_cast<int>(header->stride);
    const uint8_t* pixels = FrameSlot(header, slot);
    if (width == 0 || height == 0 || stride < static_cast<int>(width * 4)) {
        return keepLastFrame();
    }

    if (!target.texture || target.width != width || target.height != height) {
        target.texture.Reset();
        target.view.Reset();
        target.width = width;
        target.height = height;
        D3D11_TEXTURE2D_DESC texture = {};
        texture.Width = width;
        texture.Height = height;
        texture.MipLevels = 1;
        texture.ArraySize = 1;
        texture.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texture.SampleDesc.Count = 1;
        texture.Usage = D3D11_USAGE_DYNAMIC;
        texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(g_device->CreateTexture2D(&texture, nullptr, &target.texture))) {
            return false;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
        view.Format = texture.Format;
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = 1;
        if (FAILED(g_device->CreateShaderResourceView(
                target.texture.Get(),
                &view,
                &target.view))) {
            return false;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(g_context->Map(
            target.texture.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped))) {
        return keepLastFrame();
    }
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    if (mapped.RowPitch == static_cast<UINT>(stride)) {
        std::memcpy(mapped.pData, pixels, rowBytes * height);
    } else {
        for (uint32_t row = 0; row < height; ++row) {
            std::memcpy(
                static_cast<uint8_t*>(mapped.pData) +
                    static_cast<size_t>(row) * mapped.RowPitch,
                pixels + static_cast<size_t>(row) * stride,
                rowBytes);
        }
    }
    g_context->Unmap(target.texture.Get(), 0);
    const LONG after = header->slotSequence[slot];
    const LONG activeSlot = InterlockedCompareExchange(
        &header->publishedSlot,
        0,
        0);
    // The slot can advance while it is being copied. Skipping this draw causes
    // visible HUD flicker at high frame rates, so keep the copied frame and
    // let the next sequence publish replace it.
    (void)before;
    (void)after;
    (void)activeSlot;
    target.sequence = sequence;
    return keepLastFrame();
}

}  // namespace

void Present(void* swapChainPointer) {
    auto* swapChain = static_cast<IDXGISwapChain*>(swapChainPointer);
    if (!swapChain || !EnsureDevice(swapChain) || !EnsurePipeline()) {
        return;
    }
    const auto browsers = api::VisibleBrowsers();
    if (browsers.empty()) {
        return;
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer))) {
        return;
    }
    D3D11_TEXTURE2D_DESC targetDescription = {};
    backBuffer->GetDesc(&targetDescription);
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetDescription = {};
    renderTargetDescription.Format = targetDescription.Format;
    renderTargetDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetDescription.Texture2D.MipSlice = 0;
    ComPtr<ID3D11RenderTargetView> target;
    if (FAILED(g_device->CreateRenderTargetView(
            backBuffer.Get(),
            &renderTargetDescription,
            &target))) {
        return;
    }
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(targetDescription.Width);
    viewport.Height = static_cast<float>(targetDescription.Height);
    viewport.MaxDepth = 1.0f;

    std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>
        previousTargets = {};
    ComPtr<ID3D11DepthStencilView> previousDepth;
    g_context->OMGetRenderTargets(
        static_cast<UINT>(previousTargets.size()),
        previousTargets.data(),
        &previousDepth);
    UINT previousViewportCount = 0;
    g_context->RSGetViewports(&previousViewportCount, nullptr);
    std::vector<D3D11_VIEWPORT> previousViewports(previousViewportCount);
    if (previousViewportCount != 0) {
        g_context->RSGetViewports(
            &previousViewportCount,
            previousViewports.data());
    }
    ComPtr<ID3D11BlendState> previousBlend;
    float previousBlendFactor[4] = {};
    UINT previousSampleMask = 0;
    g_context->OMGetBlendState(
        &previousBlend,
        previousBlendFactor,
        &previousSampleMask);
    ComPtr<ID3D11RasterizerState> previousRasterizer;
    g_context->RSGetState(&previousRasterizer);
    ComPtr<ID3D11VertexShader> previousVertexShader;
    ComPtr<ID3D11PixelShader> previousPixelShader;
    g_context->VSGetShader(&previousVertexShader, nullptr, nullptr);
    g_context->PSGetShader(&previousPixelShader, nullptr, nullptr);
    D3D11_PRIMITIVE_TOPOLOGY previousTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    g_context->IAGetPrimitiveTopology(&previousTopology);
    ComPtr<ID3D11Buffer> previousVertexConstants;
    ComPtr<ID3D11Buffer> previousPixelConstants;
    ComPtr<ID3D11SamplerState> previousSampler;
    ComPtr<ID3D11ShaderResourceView> previousResource;
    g_context->VSGetConstantBuffers(0, 1, &previousVertexConstants);
    g_context->PSGetConstantBuffers(0, 1, &previousPixelConstants);
    g_context->PSGetSamplers(0, 1, &previousSampler);
    g_context->PSGetShaderResources(0, 1, &previousResource);

    ID3D11RenderTargetView* targets[] = {target.Get()};
    g_context->OMSetRenderTargets(1, targets, nullptr);
    g_context->RSSetViewports(1, &viewport);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);
    g_context->PSSetSamplers(0, 1, g_sampler.GetAddressOf());
    g_context->RSSetState(g_rasterizer.Get());
    const float blendFactor[4] = {0, 0, 0, 0};
    g_context->OMSetBlendState(g_blend.Get(), blendFactor, 0xffffffff);

    std::unordered_set<uint32_t> alive;
    for (const RenderBrowser& browser : browsers) {
        if (!browser.record) {
            continue;
        }
        alive.insert(browser.id);
        TextureState& texture = g_textures[browser.id];
        const auto frame = api::FrameFor(browser.id);
        if (!UploadFrame(texture, frame) || !texture.view) {
            continue;
        }
        ShaderParams params = {
            {-1.0f, 1.0f, 1.0f, -1.0f},
            browser.opacity,
            {0, 0, 0}};
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(g_context->Map(
                g_params.Get(),
                0,
                D3D11_MAP_WRITE_DISCARD,
                0,
                &mapped))) {
            continue;
        }
        std::memcpy(mapped.pData, &params, sizeof(params));
        g_context->Unmap(g_params.Get(), 0);
        g_context->VSSetConstantBuffers(0, 1, g_params.GetAddressOf());
        g_context->PSSetConstantBuffers(0, 1, g_params.GetAddressOf());
        g_context->PSSetShaderResources(0, 1, texture.view.GetAddressOf());
        g_context->Draw(3, 0);
    }
    g_context->OMSetRenderTargets(
        static_cast<UINT>(previousTargets.size()),
        previousTargets.data(),
        previousDepth.Get());
    if (previousViewportCount != 0) {
        g_context->RSSetViewports(
            previousViewportCount,
            previousViewports.data());
    } else {
        g_context->RSSetViewports(0, nullptr);
    }
    g_context->OMSetBlendState(previousBlend.Get(), previousBlendFactor, previousSampleMask);
    g_context->RSSetState(previousRasterizer.Get());
    g_context->IASetPrimitiveTopology(previousTopology);
    g_context->VSSetShader(previousVertexShader.Get(), nullptr, 0);
    g_context->PSSetShader(previousPixelShader.Get(), nullptr, 0);
    g_context->VSSetConstantBuffers(0, 1, previousVertexConstants.GetAddressOf());
    g_context->PSSetConstantBuffers(0, 1, previousPixelConstants.GetAddressOf());
    g_context->PSSetSamplers(0, 1, previousSampler.GetAddressOf());
    g_context->PSSetShaderResources(0, 1, previousResource.GetAddressOf());

    for (ID3D11RenderTargetView* target : previousTargets) {
        if (target) {
            target->Release();
        }
    }

    for (auto iterator = g_textures.begin(); iterator != g_textures.end();) {
        iterator = alive.contains(iterator->first)
            ? std::next(iterator)
            : g_textures.erase(iterator);
    }
}

void Shutdown() {
    g_textures.clear();
    g_params.Reset();
    g_rasterizer.Reset();
    g_blend.Reset();
    g_sampler.Reset();
    g_pixelShader.Reset();
    g_vertexShader.Reset();
    g_context.Reset();
    g_device.Reset();
    g_failed = false;
}

}  // namespace asterui::renderer
