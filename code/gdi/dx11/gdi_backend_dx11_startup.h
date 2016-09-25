#pragma once

#include <util/type.h>
#include <d3d11.h>
#include <dxgi.h>

struct bxGdiDeviceBackend;
struct bxGdiContextBackend;

namespace bx{
namespace gdi{

    bxGdiDeviceBackend* newDeveice_dx11( ID3D11Device* deviceDx11 );
    bxGdiContextBackend* newContext_dx11( ID3D11DeviceContext* contextDx11, IDXGISwapChain* swapChain );
    int startup_dx11( bxGdiDeviceBackend** dev, uptr hWnd, int winWidth, int winHeight, int fullScreen );

}}///