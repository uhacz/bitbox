#include "gdi_context.h"
#include "gdi_backend.h"

namespace bxGdi
{
    ///
    ///
    inline void inputLayout_create( InputLayout* il, bxGdiVertexStreamDesc* descs, int nDescs )
    {
        il->hash._0 = 0; il->hash._1 = 0; il->hash._2 = 0; il->hash._3 = 0;
        for ( int i = 0; i < nDescs; ++i )
        {
            const bxGdiVertexStreamDesc& streamDesc = descs[i];
            for ( int iblock = 0; iblock < streamDesc.count; ++iblock )
            {
                const bxGdiVertexStreamBlock block = streamDesc.blocks[iblock];
                InputLayout_SlotDesc& slot = il->slots[block.slot];
                SYS_ASSERT( slot.hash == 0 );

                slot.dataType = block.dataType;
                slot.nElement = block.numElements;
                slot.bufferIdx = i;
            }
        }
    }
    inline bool operator == (const InputLayout& iLay0, const InputLayout& iLay1)
    {
        return iLay0.hash._0 == iLay1.hash._0 &&
            iLay0.hash._1 == iLay1.hash._1 &&
            iLay0.hash._2 == iLay1.hash._2 &&
            iLay0.hash._3 == iLay1.hash._3;
    }
    inline bool operator != (const InputLayout& iLay0, const InputLayout& iLay1)
    {
        return !(iLay0 == iLay1);
    }
    ///
    ///
struct ContextPriv
{
    static StateDiff _StateDiff( const StateData& current, const StateData& pending )
    {
        StateDiff diff;

        diff.vstreams = memcmp( current._vstreams, pending._vstreams, sizeof(current._vstreams) );
        diff.vstreams |= current._count.vstreams != pending._count.vstreams;

        //diff.vertexFormat = current._inputLayout != pending._inputLayout;
        diff.vertexFormat |= current._vertexInputMask != pending._vertexInputMask;
        diff.vertexFormat |= diff.vstreams;

        diff.istream = current._istream.id != pending._istream.id;

        diff.shaders = memcmp( current._shaders, pending._shaders, sizeof(current._shaders) );
        //diff.vertexFormat |= current._shaders[eSTAGE_VERTEX].id != pending._shaders[eSTAGE_VERTEX].id;

        for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
        {
            const u8 difference = memcmp( current._textures[istage], pending._textures[istage], sizeof(current._textures[istage] ) ) != 0;
            diff.textures |= difference * BIT_OFFSET( istage );
        }
        for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
        {
            const u8 difference = memcmp( current._samplers[istage], pending._samplers[istage], sizeof(current._samplers[istage] ) ) != 0;
            diff.samplers |= difference * BIT_OFFSET( istage );
        }
        for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
        {
            const u8 difference = memcmp( current._cbuffers[istage], pending._cbuffers[istage], sizeof(current._cbuffers[istage] ) ) != 0;
            diff.cbuffers |= difference * BIT_OFFSET( istage );
        }
        diff.hwState_blend = current._hwstate.blend.key != pending._hwstate.blend.key;
        diff.hwState_depth = current._hwstate.depth.key != pending._hwstate.depth.key;
        diff.hwState_raster = current._hwstate.raster.key != pending._hwstate.raster.key;

        diff.colorRT = current._mainFramebuffer != pending._mainFramebuffer;
        diff.depthRT = current._mainFramebuffer != pending._mainFramebuffer;
        diff.colorRT |= memcmp( current._colorRT, pending._colorRT, sizeof( current._colorRT ) );
        diff.depthRT |= current._depthRT.id != pending._depthRT.id;
        diff.viewport = !equal( current._viewport, pending._viewport );
        diff.clearColor = pending._clearColor.flags != 0;

        return diff;
    }

    static StateInfo _StateInfo( const StateData& sdata )
    {
        StateInfo sinfo;

        for( int i = 0; i < eSTAGE_COUNT; ++i )
        {
            const bxGdiShader program = sdata._shaders[i];
            const unsigned bit = program.id != 0;
            const unsigned stage = (program.id) ? i : 0;
            sinfo.activeStageMask |= bit << stage;
        }

        sinfo.activeVertexSlotMask = sdata._vertexInputMask;

        return sinfo;
    }

    static void _SubmitState( bxGdiContext* ctx, const StateData& current, const StateData& pending, StateDiff diff, StateInfo sinfo )
    {
        bxGdiContextBackend* ctxBackend = ctx->_ctx;
        if( diff.colorRT || diff.depthRT )
        {
            if( pending._mainFramebuffer )
            {
                ctxBackend->changeToMainFramebuffer();
            }
            else
            {
                unsigned found_stages = 0;

                for( int icolor = 0; icolor < pending._count.colorRT; ++icolor )
                {
                    const bxGdiTexture rt = pending._colorRT[icolor];
                    for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
                    {
                        const unsigned stage_mask = BIT_OFFSET( istage );
                        for( int islot = 0; islot < cMAX_TEXTURES; ++islot )
                        {
                            const bxGdiTexture tex = current._textures[istage][islot];
                            found_stages |= ( ( tex.id == rt.id ) || ( tex.id == pending._depthRT.id ) ) * stage_mask;
                            if( found_stages & stage_mask )
                            {
                                break;
                            }
                        }
                    }
                }
                if( found_stages )
                {
                    for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
                    {
                        const unsigned stage_mask = BIT_OFFSET( istage );
                        if( found_stages & stage_mask )
                        {
                            ctxBackend->setTextures( 0, 0, cMAX_TEXTURES, istage );
                            diff.textures |= stage_mask;
                        }
                    }
                }
				ctx->changeRenderTargets((bxGdiTexture*)pending._colorRT, pending._count.colorRT, pending._depthRT);
            }
        }

        if( diff.viewport )
        {
			ctx->setViewport( pending._viewport );
        }

        if( diff.clearColor )
        {
            const ClearColorData& ccd = pending._clearColor;
            ctxBackend->clearBuffers( (bxGdiTexture*)pending._colorRT, pending._count.colorRT, pending._depthRT, (float*)ccd.rgbad, ccd.flag_clearColor, ccd.flag_clearDepth );
        }

        if( diff.shaders )
        {
            ctxBackend->setShaderPrograms( (bxGdiShader*)pending._shaders, eSTAGE_COUNT );
        }

        if( diff.vstreams || diff.vertexFormat )
        {
            bxGdiVertexBuffer vbuffers[cMAX_VERTEX_BUFFERS];
            for( int i = 0; i < cMAX_VERTEX_BUFFERS; ++i )
            {
                const bxGdiVertexBuffer vstream = pending._vstreams[i];
                const unsigned stream_slot_mask = (vstream.id) ? bxGdi::streamSlotMask( vstream.desc ) : 0;
                const unsigned bit = sinfo.activeVertexSlotMask & stream_slot_mask;
                vbuffers[i] = (bit) ? vstream : bxGdiVertexBuffer();
            }
            ctxBackend->setVertexBuffers( vbuffers, 0, cMAX_VERTEX_BUFFERS );
        }

        if( diff.istream )
        {
            ctxBackend->setIndexBuffer( pending._istream );
        }

        if( diff.vertexFormat )
        {
            bxGdiVertexStreamDesc descs[cMAX_VERTEX_BUFFERS];
            int counter = 0;
            for ( unsigned i = 0; i < pending._count.vstreams; ++i )
            {
                const bxGdiVertexStreamDesc& d = pending._vstreams[i].desc;
                int blockCount = 0;
                for ( int iblock = 0; iblock < d.count; ++iblock )
                {
                    bxGdiVertexStreamBlock block = d.blocks[iblock];
                    if ( (1 << block.slot) & sinfo.activeVertexSlotMask )
                    {
                        descs[counter].blocks[blockCount++] = block;
                    }
                }
                SYS_ASSERT( blockCount < bxGdiVertexStreamDesc::eMAX_BLOCKS );
                descs[counter].count = blockCount;
                counter += blockCount > 0;
            }

            InputLayout inputLayout;
            inputLayout_create( &inputLayout, descs, counter );

            if( inputLayout != current._inputLayout )
            {
                int found = -1;
                for ( int ilayout = 0; ilayout < ctx->_numInLayouts; ++ilayout )
                {
                    if ( ctx->_inLayoytsKey[ilayout] == pending._inputLayout )
                    {
                        found = ilayout;
                        break;
                    }
                }

                if ( found == -1 )
                {
                    SYS_ASSERT( ctx->_numInLayouts < eMAX_INPUT_LAYOUTS );
                    bxGdiInputLayout inputLayoutValue = ctx->_dev->createInputLayout( descs, counter, pending._shaders[bxGdi::eSTAGE_VERTEX] );
                    found = ctx->_numInLayouts++;
                    ctx->_inLayoytsKey[found] = inputLayout;
                    ctx->_inLayoutsValue[found] = inputLayoutValue;
                }
                ctxBackend->setInputLayout( ctx->_inLayoutsValue[found] );
            }
        }

        if( diff.textures )
        {
            for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
            {
                const u16 stage_mask = BIT_OFFSET( istage );
                if( ( stage_mask & diff.textures ) == 0 )
                    continue;

                ctx->setTextures( (bxGdiTexture*)pending._textures[istage], 0, cMAX_TEXTURES, istage );
            }
        }

        if( diff.samplers )
        {
            for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
            {
                const u16 stage_mask = BIT_OFFSET( istage );
                if( ( stage_mask & diff.samplers ) == 0 )
                    continue;

                bxGdiSampler samplers[cMAX_SAMPLERS];
                for( int isampler = 0; isampler < cMAX_SAMPLERS; ++isampler )
                {
                    const bxGdiSamplerDesc desc = pending._samplers[istage][isampler];
                    if( desc.key == 0 )
                    {
                        samplers[isampler].id = 0;
                        continue;
                    }

                    hashmap_t::cell_t* found = hashmap::lookup( ctx->_map_samplers, desc.key );
                    if (!found)
                    {
                        bxGdiSampler s = ctx->_dev->createSampler( desc );
                        found = hashmap::insert( ctx->_map_samplers, desc.key );
                        found->value = s.id;
                    }

                    samplers[isampler].id = found->value;
                }
                ctxBackend->setSamplers( samplers, 0, cMAX_SAMPLERS, istage );
            }
        }

        if( diff.cbuffers )
        {
            for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
            {
                const u16 stage_mask = BIT_OFFSET( istage );
                if( ( stage_mask & diff.cbuffers ) == 0 )
                    continue;

                ctx->setCbuffers( (bxGdiBuffer*)pending._cbuffers[istage], 0, cMAX_CBUFFERS, istage );
            }
        }

        if( diff.hwState_blend )
        {
            bxGdiHwStateDesc::Blend stateDesc = pending._hwstate.blend;
            hashmap_t::cell_t* cell = hashmap::lookup( ctx->_map_blendState, u64( stateDesc.key ) );
            if( !cell )
            {
                bxGdiBlendState state = ctx->_dev->createBlendState( stateDesc );
                cell = hashmap::insert( ctx->_map_blendState, u64( stateDesc.key ) );
                cell->value = state.id;
            }

            bxGdiBlendState state;
            state.id = cell->value;
            ctxBackend->setBlendState( state );
        }

        if (diff.hwState_depth )
        {
            bxGdiHwStateDesc::Depth stateDesc = pending._hwstate.depth;
            hashmap_t::cell_t* cell = hashmap::lookup( ctx->_map_depthState, stateDesc.key );
            if( !cell )
            {
                bxGdiDepthState state = ctx->_dev->createDepthState( stateDesc );
                cell = hashmap::insert( ctx->_map_depthState, stateDesc.key );
                cell->value = state.id;
            }

            bxGdiDepthState state;
            state.id = cell->value;
            ctxBackend->setDepthState( state );
        }

        if( diff.hwState_raster )
        {
            bxGdiHwStateDesc::Raster stateDesc = pending._hwstate.raster;
            hashmap_t::cell_t* cell = hashmap::lookup( ctx->_map_rasterState, stateDesc.key );
            if( !cell )
            {
                bxGdiRasterState state = ctx->_dev->createRasterState( stateDesc );
                cell = hashmap::insert( ctx->_map_rasterState, stateDesc.key );
                cell->value = state.id;
            }

            bxGdiRasterState state;
            state.id = cell->value;
            ctxBackend->setRasterState( state );
        }
    }
    ///
    ///
    static void _PrepareDraw( bxGdiContext* ctx )
    {
        const StateDiff sdiff = _StateDiff( ctx->current, ctx->pending );
        const StateInfo sinfo = _StateInfo( ctx->pending );

        _SubmitState( ctx, ctx->current, ctx->pending, sdiff, sinfo );  

        if( ctx->pending._topology != ctx->current._topology )
        {
            ctx->_ctx->setTopology( ctx->pending._topology );
        }

        ctx->pending._clearColor.reset();
        ctx->current = ctx->pending;
    }
};
}///

void bxGdiContext::_Startup(bxGdiDeviceBackend* dev)
{
    _dev = dev;
    _ctx = dev->ctx;
}

void bxGdiContext::_Shutdown()
{
    {
        hashmap::iterator it( _map_samplers );
        while ( it.next() )
        {
            bxGdiSampler resource;
            resource.id = it->value;
            _dev->releaseSampler( &resource );
        }
        hashmap::clear( _map_samplers );
    }
    {
        hashmap::iterator it( _map_rasterState );
        while ( it.next() )
        {
            bxGdiRasterState resource;
            resource.id = it->value;
            _dev->releaseRasterState( &resource );
        }
        hashmap::clear( _map_rasterState );
    }
    {
        hashmap::iterator it( _map_depthState );
        while ( it.next() )
        {
            bxGdiDepthState resource;
            resource.id = it->value;
            _dev->releaseDepthState( &resource );
        }
        hashmap::clear( _map_depthState );
    }
    {
        hashmap::iterator it( _map_blendState );
        while ( it.next() )
        {
            bxGdiBlendState resource;
            resource.id = it->value;
            _dev->releaseBlendState( &resource );
        }
        hashmap::clear( _map_blendState );
    }
    {
        for ( int i = 0; i < _numInLayouts; ++i )
        {
            _dev->releaseInputLayout( &_inLayoutsValue[i] );
        }
        memset( _inLayoytsKey, 0, sizeof( _inLayoytsKey ) );
        _numInLayouts = 0;
    }
}

void bxGdiContext::clear()
{
    pending.clear();
    current.clear();

    _ctx->clearState();
}

void bxGdiContext::setViewport(const bxGdiViewport& vp)
{
    pending._viewport = vp;
}

void bxGdiContext::setVertexBuffers(bxGdiVertexBuffer* vbuffers, unsigned n)
{
    SYS_ASSERT( n <= bxGdi::cMAX_VERTEX_BUFFERS );

    memset( pending._vstreams, 0, sizeof( pending._vstreams ) );
    memcpy( pending._vstreams, vbuffers, n * sizeof(*vbuffers) );

    pending._count.vstreams = n;
}

void bxGdiContext::setIndexBuffer(bxGdiIndexBuffer ibuffer)
{
    pending._istream = ibuffer;
}

void bxGdiContext::setShaders(bxGdiShader* shaders, int n, unsigned vertex_input_mask)
{
    SYS_ASSERT( n < bxGdi::eSTAGE_COUNT );
    memset( pending._shaders, 0, sizeof( pending._shaders ) );
    memcpy( pending._shaders, shaders, n * sizeof(*shaders) );
    pending._vertexInputMask = vertex_input_mask;
}

void bxGdiContext::setCbuffers(bxGdiBuffer* cbuffers, unsigned start_slot, unsigned n, int stage)
{
    SYS_ASSERT( start_slot + n <= bxGdi::cMAX_CBUFFERS );
    bxGdiBuffer* begin = &pending._cbuffers[stage][start_slot];
    memcpy( begin, cbuffers, n * sizeof(*cbuffers) );
}

void bxGdiContext::setTextures(bxGdiTexture* textures, unsigned start_slot, unsigned n, int stage)
{
    SYS_ASSERT( start_slot + n <= bxGdi::cMAX_TEXTURES );
    bxGdiTexture* begin = &pending._textures[stage][start_slot];
    memcpy( begin, textures, n * sizeof(*textures) );
}

void bxGdiContext::setSamplers(bxGdiSamplerDesc* samplers, unsigned start_slot, unsigned n, int stage)
{
    SYS_ASSERT( start_slot + n <= bxGdi::cMAX_SAMPLERS );
    bxGdiSamplerDesc* begin = &pending._samplers[stage][start_slot];
    memcpy( begin, samplers, n * sizeof(*samplers) );
}

void bxGdiContext::setCbuffer(bxGdiBuffer cbuffer, int slot, unsigned stage_mask)
{
    SYS_ASSERT( slot < bxGdi::cMAX_CBUFFERS );
    for( int istage = 0; istage < bxGdi::eSTAGE_COUNT; ++istage )
    {
        const u32 currentStageMask = BIT_OFFSET( istage );
        if( stage_mask & currentStageMask )
        {
            pending._cbuffers[istage][slot] = cbuffer;
        }
    }
}

void bxGdiContext::setTexture(bxGdiTexture texture, int slot, unsigned stage_mask)
{
    SYS_ASSERT( slot < bxGdi::cMAX_TEXTURES );
    for( int istage = 0; istage < bxGdi::eSTAGE_COUNT; ++istage )
    {
        const u32 currentStageMask = BIT_OFFSET( istage );
        if( stage_mask & currentStageMask )
        {
            pending._textures[istage][slot] = texture;
        }
    }
}

void bxGdiContext::setSampler(const bxGdiSamplerDesc& sampler, int slot, unsigned stage_mask)
{
    SYS_ASSERT( slot < bxGdi::cMAX_SAMPLERS );
    for( int istage = 0; istage < bxGdi::eSTAGE_COUNT; ++istage )
    {
        const u32 currentStageMask = BIT_OFFSET( istage );
        if( stage_mask & currentStageMask )
        {
            pending._samplers[istage][slot] = sampler;
        }
    }
}

void bxGdiContext::setHwState(const bxGdiHwStateDesc& hwstate)
{
    pending._hwstate = hwstate;
}

void bxGdiContext::setTopology(int topology)
{
    pending._topology = topology;
}

void bxGdiContext::clearTextures()
{
    memset( pending._textures, 0, sizeof( pending._textures ) );
}

void bxGdiContext::clearSamplers()
{
    memset( pending._samplers, 0, sizeof( pending._samplers ) );
}

void bxGdiContext::changeToMainFramebuffer()
{
    memset( pending._colorRT, 0, sizeof( pending._colorRT ) );
    pending._depthRT.id = 0;
    pending._count.colorRT = 0;
    pending._mainFramebuffer = 1;

    bxGdiTexture backBuffer = _ctx->backBufferTexture();
    pending._viewport = bxGdiViewport( 0, 0, backBuffer.width, backBuffer.height );
}

void bxGdiContext::changeRenderTargets(bxGdiTexture* color_rts, unsigned n_rt, bxGdiTexture depth_rt)
{
    SYS_ASSERT( n_rt <= bxGdi::cMAX_RENDER_TARGETS );
    memset( pending._colorRT, 0, sizeof(pending._colorRT) );

    memcpy( pending._colorRT, color_rts, n_rt * sizeof(*color_rts) );
    pending._depthRT = depth_rt;
    pending._count.colorRT = n_rt;
    pending._mainFramebuffer = 0;
}

void bxGdiContext::clearBuffers(float rgbad[5], int flag_color, int flag_depth)
{
    pending._clearColor.set( rgbad, flag_color, flag_depth );
}

void bxGdiContext::clearBuffers(float r, float g, float b, float a, float d, int flag_color, int flag_depth)
{
    pending._clearColor.set( r, g, b, a, d, flag_color, flag_depth );
}

void bxGdiContext::draw(unsigned num_vertices, unsigned start_index)
{
    bxGdi::ContextPriv::_PrepareDraw( this );
    _ctx->draw( num_vertices, start_index );
}

void bxGdiContext::drawIndexed(unsigned num_indices, unsigned start_index, unsigned base_vertex)
{
    bxGdi::ContextPriv::_PrepareDraw( this );
    _ctx->drawIndexed( num_indices, start_index, base_vertex );
}

void bxGdiContext::drawInstanced(unsigned num_vertices, unsigned start_index, unsigned num_instances)
{
    bxGdi::ContextPriv::_PrepareDraw( this );
    _ctx->drawInstanced( num_vertices, start_index, num_instances );
}

void bxGdiContext::drawIndexedInstanced(unsigned num_indices, unsigned start_index, unsigned num_instances, unsigned base_vertex)
{
    bxGdi::ContextPriv::_PrepareDraw( this );
    _ctx->drawIndexedInstanced( num_indices, start_index, num_instances, base_vertex );
}

///
///

namespace bxGdi
{
    enum EDrawCallCommand
    {
        eDCALL_CMD_VBUFFERS = 0xD6A1,
        eDCALL_CMD_IBUFFER,
        eDCALL_CMD_SHADERS,
        eDCALL_CMD_CBUFFERS,
        eDCALL_CMD_TEXTURES,
        eDCALL_CMD_SAMPLERS,
        eDCALL_CMD_HWSTATE,
    };

    union DrawCall_ResourceBinding
    {
        u16 hash;
        struct
        {
            u16 startSlot : 5;
            u16 numSlots : 5;
            u16 stage : 6;
        };
    };

    //struct DrawCall_Submit
    //{
    //    u8* begin;
    //    u32 numInstances;
    //    u32 __padding[1];
    //    Matrix4 worldMatrices[1];
    //};

    inline u8* _DrawCall_cmdBegin( bxGdiDrawCall* dcall )
    {
        return (u8*)dcall + sizeof( bxGdiDrawCall ) + dcall->_size;
    }
    static const int cSTRIDE_DCALL_CMD = sizeof( u16 );

    template< class T >
    inline void _DrawCall_appendCmd( bxGdiDrawCall* dcall, const T* cmdData, int nCmdData, DrawCall_ResourceBinding binding, u16 cmd )
    {
        const int cmdDataSize = nCmdData * sizeof( T );
        u8* dst = _DrawCall_cmdBegin( dcall );
        *(u16*)dst = cmd;
        dst += cSTRIDE_DCALL_CMD;
        *(DrawCall_ResourceBinding*)dst = binding;
        dst += sizeof( DrawCall_ResourceBinding );
        memcpy( dst, cmdData, cmdDataSize );
        dcall->_size += cSTRIDE_DCALL_CMD + cmdDataSize;
    }
}//



void bxGdiDrawCall::setVertexBuffers(bxGdiVertexBuffer* buffers, int nBuffers)
{
    bxGdi::DrawCall_ResourceBinding binding;
    binding.startSlot = 0;
    binding.numSlots = nBuffers;
    binding.stage = -1;
    bxGdi::_DrawCall_appendCmd( this, buffers, nBuffers, binding, bxGdi::eDCALL_CMD_VBUFFERS );
}

void bxGdiDrawCall::setIndexBuffer(bxGdiIndexBuffer ibuffer)
{
    bxGdi::DrawCall_ResourceBinding binding;
    binding.startSlot = 0;
    binding.numSlots = 1;
    binding.stage = -1;
    bxGdi::_DrawCall_appendCmd( this, &ibuffer, 1, binding, bxGdi::eDCALL_CMD_IBUFFER );
}

void bxGdiDrawCall::setShaders(bxGdiShader* shaders, int n)
{
    bxGdi::DrawCall_ResourceBinding binding;
    binding.startSlot = 0;
    binding.numSlots = n;
    binding.stage = -1;
    bxGdi::_DrawCall_appendCmd( this, shaders, n, binding, bxGdi::eDCALL_CMD_SHADERS );
}

void bxGdiDrawCall::setCbuffers(bxGdiBuffer* cbuffers, unsigned startSlot, unsigned n, int stage)
{
    bxGdi::DrawCall_ResourceBinding binding;
    binding.startSlot = startSlot;
    binding.numSlots = n;
    binding.stage = stage;
    bxGdi::_DrawCall_appendCmd( this, cbuffers, n, binding, bxGdi::eDCALL_CMD_CBUFFERS );
}

void bxGdiDrawCall::setTextures(bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage)
{
    bxGdi::DrawCall_ResourceBinding binding;
    binding.startSlot = startSlot;
    binding.numSlots = n;
    binding.stage = stage;
    bxGdi::_DrawCall_appendCmd( this, textures, n, binding, bxGdi::eDCALL_CMD_TEXTURES );
}

void bxGdiDrawCall::setSamplers(bxGdiSamplerDesc* samplers, unsigned startSlot, unsigned n, int stage)
{
    bxGdi::DrawCall_ResourceBinding binding;
    binding.startSlot = startSlot;
    binding.numSlots = n;
    binding.stage = stage;
    bxGdi::_DrawCall_appendCmd( this, samplers, n, binding, bxGdi::eDCALL_CMD_SAMPLERS );
}

void bxGdiDrawCall::setHwState(const bxGdiHwStateDesc& hwState)
{
    u8* dst = bxGdi::_DrawCall_cmdBegin( this );
    *(u16*)dst = u16( bxGdi::eDCALL_CMD_HWSTATE );
    dst += bxGdi::cSTRIDE_DCALL_CMD;
    *(bxGdiHwStateDesc*)dst = hwState;
    
    _size += bxGdi::cSTRIDE_DCALL_CMD + sizeof( bxGdiHwStateDesc );
}

bxGdiCommandBuffer* bxGdiCommandBuffer::create( u32 numBitsPerKey, u32 maxDrawCalls, bxAllocator* allocator )
{
    SYS_STATIC_ASSERT( sizeof( bxGdi::DrawCall_ResourceBinding ) == 2 );
    int memSizePerDrawCall = 0;
    memSizePerDrawCall += bxGdi::cMAX_VERTEX_BUFFERS * sizeof( bxGdiVertexBuffer );
    memSizePerDrawCall += sizeof( bxGdiIndexBuffer );
    memSizePerDrawCall += bxGdi::eDRAW_STAGES_COUNT * sizeof( bxGdiShader );
    memSizePerDrawCall += bxGdi::cMAX_TEXTURES * sizeof( bxGdiTexture );
    memSizePerDrawCall += bxGdi::cMAX_SAMPLERS * sizeof( bxGdiSamplerDesc );
    memSizePerDrawCall += bxGdi::cMAX_CBUFFERS * sizeof( bxGdiBuffer );
    memSizePerDrawCall += sizeof( bxGdiHwStateDesc );

    int memSizePerSortKey = 0;
    memSizePerSortKey += numBitsPerKey / 8;
    memSizePerSortKey += sizeof( void* );

    u32 memorySize = 0;
    memorySize += sizeof( bxGdiCommandBuffer );
    memorySize += maxDrawCalls * memSizePerDrawCall;
    memorySize += maxDrawCalls * memSizePerSortKey;
    
    void* memoryHandle = BX_MALLOC( allocator, memorySize, 8 );

    bxGdiCommandBuffer* cmdBuffer = new(memoryHandle)bxGdiCommandBuffer();

    cmdBuffer->_commandStream = (u8*)(cmdBuffer + 1);
    cmdBuffer->_sortStream = cmdBuffer->_commandStream + (memSizePerDrawCall * maxDrawCalls);
    cmdBuffer->_size_commandStream = 0;
    cmdBuffer->_size_sortStream = 0;
    cmdBuffer->_capacity_commandStream = (memSizePerDrawCall * maxDrawCalls);
    cmdBuffer->_capacity_sortStream = (memSizePerSortKey * maxDrawCalls);
    cmdBuffer->_stride_sortKey = numBitsPerKey / 8;
    cmdBuffer->_flag_activeDrawCall = 0;
    return cmdBuffer;
}

void bxGdiCommandBuffer::release(bxGdiCommandBuffer** cmdBuffer, bxAllocator* allocator)
{
    SYS_ASSERT( cmdBuffer[0]->_flag_activeDrawCall == 0 );
    cmdBuffer[0]->~bxGdiCommandBuffer();
    BX_FREE0( allocator, cmdBuffer[0] );
}

///
///
bxGdiDrawCall* bxGdiCommandBuffer::beginDrawCall()
{
    SYS_ASSERT( _flag_activeDrawCall == 0 );
    bxGdiDrawCall* dcall = (bxGdiDrawCall*)(_commandStream + _size_commandStream);
    dcall->_size = 0;

    _flag_activeDrawCall = 1;

    return dcall;
}

//void bxGdiCommandBuffer::submitDrawCall( bxGdiDrawCall* dcall, const void* key, const Matrix4* worldMatrices, int nInstances )
//{
//    SYS_ASSERT( _flag_activeDrawCall == 1 );
//    
//    u32 dcallSize = sizeof( bxGdiDrawCall ) + dcall->_size;
//    
//    a
//
//    SYS_ASSERT( _size_commandStream + dcallSize <= _capacity_commandStream );
//    SYS_ASSERT( ( _size_sortStream + _stride_sortKey + sizeof( void* ) ) <= _capacity_sortStream );
//    
//    memcpy( _sortStream + _size_sortStream, key, _stride_sortKey );
//    _size_sortStream += _stride_sortKey;
//    memcpy( _sortStream + _size_sortStream, &dcall, sizeof( void* ) );
//    _size_sortStream += _stride_sortKey;
//
//
//
//}


void bxGdiCommandBuffer::endDrawCall( bxGdiDrawCall** dcall )
{
    SYS_ASSERT( _flag_activeDrawCall == 1 );
    _flag_activeDrawCall = 0;
    dcall[0] = 0;

    
}



void bxGdiCommandBuffer::sort()
{
}

void bxGdiCommandBuffer::flush(bxGdiContext* ctx)
{
}