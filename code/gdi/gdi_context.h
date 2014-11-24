#pragma once

#include "gdi_backend.h"
#include <util/hashmap.h>

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
            : flags( 0 )
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
        void set( const float rgbad[5], int cc, int cd ) { set( rgbad[0], rgbad[1], rgbad[2], rgbad[3], rgbad[4], cc, cd ); }
        void reset()
        {
            flags = 0;
        }
    };

    enum
    {
        eMAX_INPUT_LAYOUTS = 64,
    };

    union InputLayout_SlotDesc
    {
        u64 hash;
        struct
        {
            u16 dataType : 4;
            u16 nElement : 4;
            u16 bufferIdx : 8;
        };
    };

    union InputLayout
    {
        struct
        {
            u64 _0;
            u64 _1;
            u64 _2;
            u64 _3;
        } hash;
      
        InputLayout_SlotDesc slots[eSLOT_COUNT];
    };
    
    struct StateData
    {
        InputLayout         _inputLayout;
        bxGdiVertexBuffer   _vstreams[cMAX_VERTEX_BUFFERS];
        bxGdiIndexBuffer    _istream;

        bxGdiShader        _shaders[eSTAGE_COUNT];
        bxGdiTexture       _textures[eSTAGE_COUNT][cMAX_TEXTURES];
        bxGdiSamplerDesc   _samplers[eSTAGE_COUNT][cMAX_SAMPLERS];
        bxGdiBuffer        _cbuffers[eSTAGE_COUNT][cMAX_CBUFFERS];
        bxGdiHwStateDesc   _hwstate;
        bxGdiTexture       _colorRT[cMAX_RENDER_TARGETS];
        bxGdiTexture       _depthRT;
        ClearColorData     _clearColor;
        bxGdiViewport	   _viewport;
        u32                _mainFramebuffer;
        i32				   _topology;
        u32                _vertexInputMask;

        struct  
        {
            u8 vstreams;
            u8 colorRT;
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

    struct ContextPriv;
}///


struct bxGdiContext
{
private:
    ///
    ///
    bxGdiDeviceBackend* _dev;
    bxGdiContextBackend* _ctx;

    bxGdi::StateData current;
    bxGdi::StateData pending;

    bxGdi::InputLayout _inLayoytsKey[bxGdi::eMAX_INPUT_LAYOUTS];
    bxGdiInputLayout _inLayoutsValue[bxGdi::eMAX_INPUT_LAYOUTS];

    hashmap_t _map_blendState;
    hashmap_t _map_depthState;
    hashmap_t _map_rasterState;
    hashmap_t _map_samplers;

    i32 _numInLayouts;
    
    friend struct bxGdi::ContextPriv;
public:
    /// 
    ///
    bxGdiContext()
        : _dev(0)
        , _ctx(0)
        , _numInLayouts(0)
    {
        pending._hwstate = bxGdiHwStateDesc();
        pending._viewport = bxGdiViewport();
    }

    void _Startup( bxGdiDeviceBackend* dev );
    void _Shutdown();

    bxGdiContextBackend* backend() { return _ctx; }
    
    int indicesBound() const { return pending._istream.id != 0; }

    ///
    ///
    void clear                  ();
    void setViewport            ( const bxGdiViewport& vp );
    void setVertexBuffers       ( bxGdiVertexBuffer* vbuffers, unsigned n );
    void setIndexBuffer         ( bxGdiIndexBuffer ibuffer );
    void setShaders             ( bxGdiShader* shaders, int n, unsigned vertex_input_mask );
    void setCbuffers            ( bxGdiBuffer* cbuffers, unsigned start_slot, unsigned n, int stage );
    void setTextures            ( bxGdiTexture* textures, unsigned start_slot, unsigned n, int stage );
    void setSamplers            ( bxGdiSamplerDesc* samplers, unsigned start_slot, unsigned n, int stage );
    void setCbuffer             ( bxGdiBuffer cbuffer, int slot, unsigned stage_mask );
    void setTexture             ( bxGdiTexture texture, int slot, unsigned stage_mask );
    void setSampler             ( const bxGdiSamplerDesc& sampler, int slot, unsigned stage_mask );
    void setHwState             ( const bxGdiHwStateDesc& hwstate );
    void setTopology            ( int topology );

    void clearTextures          ();
    void clearSamplers          ();

    void changeToMainFramebuffer();
    void changeRenderTargets    ( bxGdiTexture* color_rts, unsigned n_rt, bxGdiTexture depth_rt );
    void clearBuffers           ( float rgbad[5], int flag_color, int flag_depth );
    void clearBuffers           ( float r, float g, float b, float a, float d, int flag_color, int flag_depth );

    void draw                   ( unsigned num_vertices, unsigned start_index );
    void drawIndexed            ( unsigned num_indices, unsigned start_index, unsigned base_vertex );
    void drawInstanced          ( unsigned num_vertices, unsigned start_index, unsigned num_instances );
    void drawIndexedInstanced   ( unsigned num_indices, unsigned start_index, unsigned num_instances, unsigned base_vertex );
};


///
///
class bxGdiDrawCall
{
public:
    void setVertexBuffers( bxGdiVertexBuffer* buffers, int nBuffers );
    void setIndexBuffer  ( bxGdiIndexBuffer ibuffer );
    void setShaders      ( bxGdiShader* shaders, int n );
    void setCbuffers     ( bxGdiBuffer* cbuffers, unsigned startSlot, unsigned n, int stage );
    void setTextures     ( bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage );
    void setSamplers     ( bxGdiSamplerDesc* samplers, unsigned startSlot, unsigned n, int stage );
    void setHwState      ( const bxGdiHwStateDesc& hwState );

    u16 _size;
};

class bxGdiCommandBuffer
{
public:
    static bxGdiCommandBuffer* create( u32 numBitsPerKey, u32 maxDrawCalls, bxAllocator* allocator );
    static void release( bxGdiCommandBuffer** cmdBuffer, bxAllocator* allocator );

    bxGdiDrawCall* beginDrawCall();
    //void submitDrawCall( bxGdiDrawCall* dcall, const void* key, const Matrix4* worldMatrices, int nInstances );
    void endDrawCall( bxGdiDrawCall** dcall );
    
    void sort();
    void flush( bxGdiContext* ctx );

private:
    u8* _commandStream;
    u8* _submitStream;
    u8* _sortStream;

    u32 _size_commandStream;
    u32 _size_submitStream;
    u32 _size_sortStream;
    u32 _capacity_commandStream;
    u32 _capacity_submitStream;
    u32 _capacity_sortStream;

    u8 _stride_sortKey;
    u8 _flag_activeDrawCall;

};


