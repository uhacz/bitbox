#include "game_gui.h"

#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"

#include <system/window.h>
#include <rdi\rdi_backend.h>

namespace bx{ namespace game_gui{

static int ImGui_WinMsgHandler( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    ImGuiIO& io = ImGui::GetIO();
    switch( msg )
    {
    case WM_LBUTTONDOWN:
        io.MouseDown[0] = true;
        break;
    case WM_LBUTTONUP:
        io.MouseDown[0] = false;
        break;
    case WM_RBUTTONDOWN:
        io.MouseDown[1] = true;
        break;
    case WM_RBUTTONUP:
        io.MouseDown[1] = false;
        break;
    case WM_MOUSEWHEEL:
        io.MouseWheel += GET_WHEEL_DELTA_WPARAM( wParam ) > 0 ? +1.0f : -1.0f;
        break;
    case WM_MOUSEMOVE:
        // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        io.MousePos.x = (signed short)( lParam );
        io.MousePos.y = (signed short)( lParam >> 16 );
        break;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if( wParam > 0 && wParam < 0x10000 )
            io.AddInputCharacter( (unsigned short)wParam );
        break;
    }

    const bool isHovering = ImGui::IsMouseHoveringAnyWindow();
    return ( isHovering ) ? 1 : 0;
}

void StartUp()
{
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    rdi::device::GetAPIDevice( &dev, &ctx );
    ImGui_ImplDX11_Init( bxWindow_get()->hwnd, dev, ctx );
    bxWindow_addWinMsgCallback( bxWindow_get(), ImGui_WinMsgHandler );
}

void ShutDown()
{
    bxWindow_removeWinMsgCallback( bxWindow_get(), ImGui_WinMsgHandler );
    ImGui_ImplDX11_Shutdown();
}

void NewFrame()
{
    ImGui_ImplDX11_NewFrame();
}

void Render()
{
    ImGui::Render();
}

}
}//