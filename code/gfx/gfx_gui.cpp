#include "gfx_gui.h"
#include <system/window.h>

bxGfxGUI::bxGfxGUI()
    : _rsource(0)
    , _fxI(0)
{}

namespace
{
    void ImGui_init()
    {
        
    }
    void ImGui_WinMsgHandler( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {
    
    }


    struct Vertex
    {
        f32 pos[2];
        f32 uv[2];
        u32 col;
    };

    const int MAX_VERTICES = 10000;
}

void bxGfxGUI::startup( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win )
{
    _rsource = bxGdi::renderSource_new( 1 );
    bxGdiVertexStreamDesc vsDesc;
    vsDesc.addBlock( bxGdi::eSLOT_POSITION, bxGdi::eTYPE_FLOAT, 2 );
    vsDesc.addBlock( bxGdi::eSLOT_COLOR0, bxGdi::eTYPE_UBYTE, 4 );
    vsDesc.addBlock( bxGdi::eSLOT_TEXCOORD0, bxGdi::eTYPE_FLOAT, 2 );

    bxGdiVertexBuffer vbuffer = dev->createVertexBuffer( vsDesc, MAX_VERTICES );
    bxGdi::renderSource_setVertexBuffer( _rsource, vbuffer, 0 );

    _fxI = bxGdi::shaderFx_createWithInstance( dev, resourceManager, "gui" );
    SYS_ASSERT( _fxI != 0 );

    _cbuffer = dev->createConstantBuffer( sizeof( Matrix4 ) );

    bxWindow_addWinMsgCallback( win, ImGui_WinMsgHandler );
}

void bxGfxGUI::shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxWindow* win )
{
    bxWindod_removeWinMsgCallback( win, ImGui_WinMsgHandler );

    dev->releaseBuffer( &_cbuffer );
    bxGdi::shaderFx_releaseWithInstance( dev, &_fxI );
    bxGdi::renderSource_releaseAndFree( dev, &_rsource );
}

void bxGfxGUI::update( float deltaTime )
{

}

void bxGfxGUI::draw( bxGfxContext* ctx )
{

}
