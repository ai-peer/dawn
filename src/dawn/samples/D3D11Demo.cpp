
// This sample demonstrates the use of the D3D11 API to render a triangle.
// It is based on the Direct3D 11 sample code from the MSDN website:
// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476876(v=vs.85).aspx
//
// The sample is a port of the D3D11 sample from the Dawn project:
//

#include <assert.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <wrl/client.h>

#include <vector>

#include "GLFW/glfw3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

using Microsoft::WRL::ComPtr;

static GLFWwindow* g_window = nullptr;

ComPtr<ID3D11Device> g_pd3dDevice;
ComPtr<ID3D11DeviceContext> g_pd3dDeviceContext;
ComPtr<IDXGISwapChain> g_pSwapChain;
ComPtr<ID3D11RenderTargetView> g_mainRenderTargetView;
ComPtr<ID3D11InputLayout> g_pInputLayout;
ComPtr<ID3D11Buffer> g_pVertexBuffer;

// shaders
ComPtr<ID3D11VertexShader> g_pVertexShader;
ComPtr<ID3D11PixelShader> g_pPixelShader;

void InitDemo() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    g_window = glfwCreateWindow(1024, 720, "D3D11", nullptr, nullptr);

    HRESULT hr;

    // Create the D3D11 device
    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevelArray[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1};
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                           featureLevelArray, std::size(featureLevelArray), D3D11_SDK_VERSION,
                           &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    assert(SUCCEEDED(hr));

    // Create the swap chain
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = glfwGetWin32Window(g_window);
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;

    ComPtr<IDXGIDevice> dxgiDevice;
    g_pd3dDevice.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
    ComPtr<IDXGIFactory> dxgiFactory;
    dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);

    auto result = dxgiFactory->CreateSwapChain(g_pd3dDevice.Get(), &sd, &g_pSwapChain);
    if (result != S_OK) {
        printf("Failed to create swap chain");
    }

    // Setup shaders
    // Create shaders from inline strings
    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;
#if 1
    const char* pVS = R"(
        struct VS_INPUT
        {
            float3 Pos : POSITION;
        };

        struct PS_INPUT
        {
            float4 Pos : SV_POSITION;
        };

        PS_INPUT main(VS_INPUT input)
        {
            PS_INPUT output;
            output.Pos = float4(input.Pos.xy, 0.0f, 1.0f);
            return output;
        })";
#else
    const char* pVS = R"(
        struct VS_INPUT
        {
            uint vertexId : SV_VertexID;
        };
        struct PS_INPUT
        {
            float4 Pos : SV_POSITION;
        };

        PS_INPUT main(VS_INPUT input)
        {
            float3 vertices[3] = {
                {-0.5f, -0.5f, 0.0f},
                {0.0f, 0.5f, 0.0f},
                {0.5f, -0.5f, 0.0f}};
            PS_INPUT output;
            output.Pos = float4(vertices[input.vertexId].xy, 0.0f, 1.0f);
            return output;
        })";
#endif

    const char* pPS = R"(
        float4 main() : SV_Target
        {
            return float4(1.0f, 0.5f, 0.2f, 1.0f);
        })";

    hr = D3DCompile(pVS, strlen(pVS), "VS", nullptr, nullptr, "main", "vs_5_0", 0, 0, &pVSBlob,
                    nullptr);
    assert(SUCCEEDED(hr));

    hr = D3DCompile(pPS, strlen(pPS), "PS", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pPSBlob,
                    nullptr);
    assert(SUCCEEDED(hr));

    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
                                          nullptr, &g_pVertexShader);
    assert(SUCCEEDED(hr));

    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                         nullptr, &g_pPixelShader);
    assert(SUCCEEDED(hr));

#if 1
    // Create the input layout
    D3D11_INPUT_ELEMENT_DESC localLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = g_pd3dDevice->CreateInputLayout(localLayout, 1, pVSBlob->GetBufferPointer(),
                                         pVSBlob->GetBufferSize(), &g_pInputLayout);
    assert(SUCCEEDED(hr));

    // Setup input layout
    g_pd3dDeviceContext->IASetInputLayout(g_pInputLayout.Get());

    // Create vertex buffer
    float vertices[] = {
        0.0f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    assert(SUCCEEDED(hr));

    UINT stride = sizeof(float) * 3;
    UINT offset = 0;
    ID3D11Buffer* pBuffers[1] = {g_pVertexBuffer.Get()};
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, pBuffers, &stride, &offset);
#endif
    // Set primitive topology
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the render target view
    ComPtr<ID3D11Texture2D> pBackBuffer;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer);
    assert(SUCCEEDED(hr));

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &g_mainRenderTargetView);
    assert(SUCCEEDED(hr));

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = 1024.0f;
    vp.Height = 728.0f;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pd3dDeviceContext->RSSetViewports(1, &vp);

    ID3D11RenderTargetView* renderTargetViewPtr = g_mainRenderTargetView.Get();
    g_pd3dDeviceContext->OMSetRenderTargets(1, &renderTargetViewPtr, nullptr);
}

// Draw a triangle using the D3D11 device
void Render() {
    static long frameCount = 0;

    frameCount++;

    // Clear the back buffer
    float ClearColor[4] = {0.0f, 0.0f, 1.0f / 256 * (frameCount % 256),
                           1.0f};  // red,green,blue,alpha
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView.Get(), ClearColor);

    g_pd3dDeviceContext->VSSetShader(g_pVertexShader.Get(), nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(g_pPixelShader.Get(), nullptr, 0);

    g_pd3dDeviceContext->Draw(3, 0);
}

int main() {
    // Setup window
    InitDemo();

    // Main loop
    while (!glfwWindowShouldClose(g_window)) {
        glfwPollEvents();
        Render();
        g_pSwapChain->Present(1, 0);  // Present with vsync
        // sleep 1 second
        // Sleep(1000);
    }
    return 0;
}