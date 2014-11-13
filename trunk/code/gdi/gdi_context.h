#pragma once

#include "gdi_backend.h"
#include <memory.h>

namespace bxGdi
{
    struct ClearColorData
    {
        float rgbad[5];
        union
        {
            u32 flags;
            struct  
            {
                u32 flag_clearColor : 1;
                u32 flag_clearDepth : 1;
            };
        };

        ClearColorData()
            : flags(0)
        {
            rgbad[0] = rgbad[1] = rgbad[2] = rgbad[3] = rgbad[4] = 0.f;
        }

        void set( float r, float g, float b, float a, float d, int cc, int cd )
        {
            rgbad[0] = r;
            rgbad[1] = g;
            rgbad[2] = b;
            rgbad[3] = a;
            rgbad[4] = d;
            flag_clearColor = cc;
            flag_clearDepth = cd;
        }
        void set( const float rgbad[5], int cc, int cd ) { set( rgbad[0], rgbad[1], rgbad[2], rgbad[3], rgbad[4], cc, cd );}
        void reset()
        {
            flags = 0;
        }
    };

    struct StateData
    {
        bxGdiVertexStreamBlock _vstreamBlocks[cMAX_VERTEX_BUFFERS*bxGdiVertexStreamDesc::eMAX_BLOCKS];
        bxGdiVertexBuffer   _vstreams[cMAX_VERTEX_BUFFERS];
        bxGdiIndexBuffer    _istream;

        bxGdiShader        _shaders[eSTAGE_COUNT];
        bxGdiTexture       _textures[eSTAGE_COUNT][cMAX_TEXTURES];
        bxGdiSamplerDesc   _samplers[eSTAGE_COUNT][cMAX_SAMPLERS];
        bxGdiBuffer        _cbuffers[eSTAGE_COUNT][cMAX_CBUFFERS];
        bxGdiHwState       _hwstate;
        bxGdiTexture       _color_rt[cMAX_RENDER_TARGETS];
        bxGdiTexture       _depth_rt;
        ClearColorData     _clear_color;
        bxGdiViewport	   _viewport;
        u32                _mainFramebuffer;
        i32				   _topology;
        u32                _vertex_input_mask;

        struct  
        {
            u8 vstreams;
            u8 vstreamBlocks;
            u8 color_rt;
        } _count;

        StateData()
        {
            clear();
        }

        void clear()
        {
            memset( this, 0, sizeof(*this) );
            _topology = -1;
        }
    };

    union StateDiff
    {
        u32 all;
        struct  
        {
            u32 vstreams       : 1;
            u32 istream        : 1;
            u32 shaders        : 1;
            u32 vertexFormat   : 1;
            u32 colorRT        : 1;
            u32 depthRT        : 1;
            u32 hwState_blend  : 1;
            u32 hwState_depth  : 1;
            u32 hwState_raster : 1;
            u32 viewport       : 1;
            u32 clearColor     : 1;
            u32 textures       : eSTAGE_COUNT;
            u32 samplers       : eSTAGE_COUNT;
            u32 cbuffers       : eSTAGE_COUNT;
        };

        StateDiff()
            : all( 0 )
        {}
    };

    struct StateInfo
    {
        u16 activeVertexSlotMask;
        u16 activeStageMask;

        StateInfo()
            : activeVertexSlotMask(0)
            , activeStageMask(0)
        {}
    };
}///

struct bxGdiContext
{
    bxGdi::StateData current;
    bxGdi::StateData pending;

    bxGdiContext()
    {
        pending._hwstate = bxGdiHwState();
        pending._viewport = bxGdiViewport();
    }

    void clear                     ();
    void setViewport               ( const bxGdiViewport& vp );
    void setVertexBuffers          ( bxGdiVertexBuffer* vbuffers, unsigned n );
    void setIndexBuffer            ( bxGdiIndexBuffer ibuffer );
    void setShaders                ( bxGdiShader* shaders, int n, unsigned vertex_input_mask );
    void setCbuffers               ( bxGdiBuffer* cbuffers, unsigned start_slot, unsigned n, int stage );
    void setTextures               ( bxGdiTexture* textures, unsigned start_slot, unsigned n, int stage );
    void setSamplers               ( bxGdiSamplerDesc* samplers, unsigned start_slot, unsigned n, int stage );
    void setCbuffer                ( bxGdiBuffer cbuffer, int slot, unsigned stage_mask );
    void setTexture                ( bxGdiTexture texture, int slot, unsigned stage_mask );
    void setSampler                ( const bxGdiSamplerDesc& sampler, int slot, unsigned stage_mask );
    void setHwState                ( const bxGdiHwState& hwstate );
    void setTopology               ( int topology );

    void clearTextures             ();
    void clearSamplers             ();

    void changeToMainFramebuffer   ();
    void changeRenderTargets       ( bxGdiTexture* color_rts, unsigned n_rt, bxGdiTexture depth_rt );
    void clearBuffers              ( float rgbad[5], int flag_color, int flag_depth );
    void clearBuffers              ( float r, float g, float b, float a, float d, int flag_color, int flag_depth );

    void draw                      ( unsigned num_vertices, unsigned start_index );
    void drawIndexed               ( unsigned num_indices, unsigned start_index, unsigned base_vertex );
    void drawInstanced             ( unsigned num_vertices, unsigned start_index, unsigned num_instances );
    void drawIndexedInstanced      ( unsigned num_indices, unsigned start_index, unsigned num_instances, unsigned base_vertex );
};