#include "gfx_gui.h"
//#include "gfx_camera.h"
#include <system/window.h>
#include <gdi/gdi_shader.h>
#include <gdi/gdi_render_source.h>
#include <gdi/gdi_context.h>


#define STB_IMAGE_IMPLEMENTATION
#include "gui/imgui/stb_image.h"
#include <util/common.h>
#include <util/hash.h>
#include <resource_manager/resource_manager.h>

using namespace bx;

struct bxGfxGUI_Impl
{
    bxGdiRenderSource* _rsource;
    bxGdiShaderFx_Instance* _fxI;
    bxGdiBuffer _cbuffer;
    bxGdiTexture _fontTexture;

    bxGfxGUI_Impl::bxGfxGUI_Impl()
        : _rsource( 0 )
        , _fxI( 0 )
    {}

    void _Startup( bxGdiDeviceBackend* dev , bxWindow* win );
    void _Shutdown( bxGdiDeviceBackend* dev, bxWindow* win );
};

namespace
{
    bxGfxGUI_Impl* __gui = 0;
    bxGdiContext* __gdiCtx = 0;

    struct Vertex
    {
        f32 pos[2];
        f32 uv[2];
        u32 col;
    };

    const int MAX_VERTICES = 100000;

    void ImGui_RenderDrawLists( ImDrawList** const cmd_lists, int cmd_lists_count )
    {
        size_t total_vtx_count = 0;
        for ( int n = 0; n < cmd_lists_count; n++ )
        {
            total_vtx_count += cmd_lists[n]->vtx_buffer.size();
        }
        if ( total_vtx_count == 0 )
        {
            return;
        }

        bxGdiContextBackend* gdiBackend = __gdiCtx->backend();

        Vertex* mappedVertices = (Vertex*)gdiBackend->mapVertices( __gui->_rsource->vertexBuffers[0], 0, (int)total_vtx_count, gdi::eMAP_WRITE );

        for ( int n = 0; n < cmd_lists_count; n++ )
        {
            const ImDrawList* cmd_list = cmd_lists[n];
            const ImDrawVert* vtx_src = &cmd_list->vtx_buffer[0];
            for ( size_t i = 0; i < cmd_list->vtx_buffer.size(); i++ )
            {
                mappedVertices->pos[0] = vtx_src->pos.x;
                mappedVertices->pos[1] = vtx_src->pos.y;
                mappedVertices->uv[0] = vtx_src->uv.x;
                mappedVertices->uv[1] = vtx_src->uv.y;
                mappedVertices->col = vtx_src->col;
                mappedVertices++;
                vtx_src++;
            }
        }
        gdiBackend->unmapVertices( __gui->_rsource->vertexBuffers[0] );

        const float L = 0.0f;
        const float R = ImGui::GetIO().DisplaySize.x;
        const float B = ImGui::GetIO().DisplaySize.y;
        const float T = 0.0f;

        const float mvp[4][4] =
        {
            { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
            { 0.0f, 2.0f / (T - B), 0.0f, 0.0f, },
            { 0.0f, 0.0f, 0.5f, 0.0f },
            { (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
        };
        __gui->_fxI->setUniform( "projMatrix", mvp );
        
        __gdiCtx->setViewport( bxGdiViewport( 0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y ) );
        __gdiCtx->setTopology( gdi::eTRIANGLES );
        gdi::renderSource_enable( __gdiCtx, __gui->_rsource );
        gdi::shaderFx_enable( __gdiCtx, __gui->_fxI, "imgui" );
        
        // Render command lists
        int vtx_offset = 0;
        for ( int n = 0; n < cmd_lists_count; n++ )
        {
            // Render command list
            const ImDrawList* cmd_list = cmd_lists[n];
            for ( size_t cmd_i = 0; cmd_i < cmd_list->commands.size(); cmd_i++ )
            {
                const ImDrawCmd* pcmd = &cmd_list->commands[cmd_i];
                const bxGdiRect r ( (LONG)pcmd->clip_rect.x, (LONG)pcmd->clip_rect.y, (LONG)pcmd->clip_rect.z, (LONG)pcmd->clip_rect.w );
                __gdiCtx->backend()->setScissorRects( &r, 1 );

                //g_pd3dDeviceImmediateContext->RSSetScissorRects( 1, &r );
                //g_pd3dDeviceImmediateContext->Draw( pcmd->vtx_count, vtx_offset );
                __gdiCtx->draw( pcmd->vtx_count, vtx_offset );
                vtx_offset += pcmd->vtx_count;
            }
        }
    }
    void ImGui_init( bxGdiTexture* fontTexture, HWND hWnd, bxGdiDeviceBackend* dev )
    {
        RECT rect;
        GetClientRect( hWnd, &rect );
        int display_w = (int)(rect.right - rect.left);
        int display_h = (int)(rect.bottom - rect.top);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2( (float)display_w, (float)display_h );    // Display size, in pixels. For clamping windows positions.
        io.DeltaTime = 1.0f / 60.0f;                                      // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our time step is variable)
        io.PixelCenterOffset = 0.0f;                                    // Align Direct3D Texels
        io.KeyMap[ImGuiKey_Tab] = VK_TAB;                               // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
        io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
        io.KeyMap[ImGuiKey_DownArrow] = VK_UP;
        io.KeyMap[ImGuiKey_Home] = VK_HOME;
        io.KeyMap[ImGuiKey_End] = VK_END;
        io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
        io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
        io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
        io.KeyMap[ImGuiKey_A] = 'A';
        io.KeyMap[ImGuiKey_C] = 'C';
        io.KeyMap[ImGuiKey_V] = 'V';
        io.KeyMap[ImGuiKey_X] = 'X';
        io.KeyMap[ImGuiKey_Y] = 'Y';
        io.KeyMap[ImGuiKey_Z] = 'Z';
        io.RenderDrawListsFn = ImGui_RenderDrawLists;

        // Load font texture
        // Default font (embedded in code)
        const void* png_data;
        unsigned int png_size;
        ImGui::GetDefaultFontData( NULL, NULL, &png_data, &png_size );
        int tex_x, tex_y, tex_comp;
        void* tex_data = stbi_load_from_memory( (const unsigned char*)png_data, (int)png_size, &tex_x, &tex_y, &tex_comp, 0 );
        SYS_ASSERT( tex_data != NULL );

        fontTexture[0] = dev->createTexture2D( tex_x, tex_y, 1, bxGdiFormat( gdi::eTYPE_UBYTE, 4, 1, 0 ), gdi::eBIND_SHADER_RESOURCE, 0, tex_data );
        SYS_ASSERT( fontTexture->id != 0 );
    }
    int ImGui_WinMsgHandler( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {
        ImGuiIO& io = ImGui::GetIO();
        switch ( msg )
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
            io.MousePos.x = (signed short)(lParam);
            io.MousePos.y = (signed short)(lParam >> 16);
            break;
        case WM_CHAR:
            // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
            if ( wParam > 0 && wParam < 0x10000 )
                io.AddInputCharacter( (unsigned short)wParam );
            break;
        }

        const bool isHovering = ImGui::IsMouseHoveringAnyWindow();
        return (isHovering) ? 1 : 0;
    }


    
}

void bxGfxGUI::_Startup( bxGdiDeviceBackend* dev, bxWindow* win )
{
    SYS_ASSERT( __gui == 0 );

    __gui = BX_NEW( bxDefaultAllocator(), bxGfxGUI_Impl );
    __gui->_Startup( dev, win );
}

void bxGfxGUI::_Shutdown( bxGdiDeviceBackend* dev, bxWindow* win )
{
    if( !__gui )
        return;

    __gui->_Shutdown( dev, win );
    BX_DELETE0( bxDefaultAllocator(), __gui );
}

void bxGfxGUI_Impl::_Startup( bxGdiDeviceBackend* dev, bxWindow* win )
{
    ResourceManager* resourceManager = bx::getResourceManager();

    _rsource = gdi::renderSource_new( 1 );
    bxGdiVertexStreamDesc vsDesc;
    vsDesc.addBlock( gdi::eSLOT_POSITION, gdi::eTYPE_FLOAT, 2 );
    vsDesc.addBlock( gdi::eSLOT_TEXCOORD0, gdi::eTYPE_FLOAT, 2 );
    vsDesc.addBlock( gdi::eSLOT_COLOR0, gdi::eTYPE_UBYTE, 4, 1 );

    bxGdiVertexBuffer vbuffer = dev->createVertexBuffer( vsDesc, MAX_VERTICES );
    gdi::renderSource_setVertexBuffer( _rsource, vbuffer, 0 );

    _fxI = gdi::shaderFx_createWithInstance( dev, resourceManager, "gui" );
    SYS_ASSERT( _fxI != 0 );

    _cbuffer = dev->createConstantBuffer( sizeof( Matrix4 ) );

    ImGui_init( &_fontTexture, win->hwnd, dev );
    _fxI->setTexture( "texture0", _fontTexture );
    _fxI->setSampler( "sampler0", bxGdiSamplerDesc( gdi::eFILTER_NEAREST ) );
    bxWindow_addWinMsgCallback( win, ImGui_WinMsgHandler );
}

void bxGfxGUI_Impl::_Shutdown( bxGdiDeviceBackend* dev, bxWindow* win )
{
    bxWindow_removeWinMsgCallback( win, ImGui_WinMsgHandler );

    ResourceManager* resourceManager = bx::getResourceManager();
    dev->releaseTexture( &_fontTexture );
    dev->releaseBuffer( &_cbuffer );
    gdi::shaderFx_releaseWithInstance( dev, resourceManager, &_fxI );
    gdi::renderSource_releaseAndFree( dev, &_rsource );

    ImGui::Shutdown();
}

void bxGfxGUI::newFrame( float deltaTime )
{
    ImGuiIO& io = ImGui::GetIO();

    deltaTime = clamp( deltaTime, 0.00001f, 0.1f );

    // Setup time step
    io.DeltaTime = deltaTime;

    // Setup inputs
    // (we already got mouse position, buttons, wheel from the window message callback)
    BYTE keystate[256];
    GetKeyboardState( keystate );
    for ( int i = 0; i < 256; i++ )
    {
        io.KeysDown[i] = (keystate[i] & 0x80) != 0;
    }
    io.KeyCtrl = (keystate[VK_CONTROL] & 0x80) != 0;
    io.KeyShift = (keystate[VK_SHIFT] & 0x80) != 0;
    ImGui::NewFrame();
}

void bxGfxGUI::draw( bxGdiContext* ctx )
{
    __gdiCtx = ctx;

    //ctx->clear();

    ImGui::Render();

    //ctx->clear();

    __gdiCtx = 0;
}
