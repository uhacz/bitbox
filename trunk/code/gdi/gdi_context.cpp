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
    StateDiff _StateDiff( const StateData& current, const StateData& pending )
    {
        StateDiff diff;

        diff.vstreams = memcmp( current._vstreams, pending._vstreams, sizeof(current._vstreams) );
        diff.vstreams |= current._count.vstreams != pending._count.vstreams;

        //diff.vertexFormat = current._inputLayout != pending._inputLayout;
        diff.vertexFormat |= current._vertex_input_mask != pending._vertex_input_mask;
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
        diff.colorRT |= memcmp( current._color_rt, pending._color_rt, sizeof( current._color_rt ) );
        diff.depthRT |= current._depth_rt.id != pending._depth_rt.id;
        diff.viewport = !equal( current._viewport, pending._viewport );
        diff.clearColor = pending._clear_color.flags != 0;

        return diff;
    }

    StateInfo state_info( const StateData& sdata )
    {
        StateInfo sinfo;

        for( int i = 0; i < eSTAGE_COUNT; ++i )
        {
            const bxGdiShader program = sdata._shaders[i];
            const unsigned bit = program.id != 0;
            const unsigned stage = (program.id) ? i : 0;
            sinfo.activeStageMask |= bit << stage;
        }

        sinfo.activeVertexSlotMask = sdata._vertex_input_mask;

        return sinfo;
    }

    void _SubmitState( bxGdiContext* ctx, const StateData& current, const StateData& pending, StateDiff diff, StateInfo sinfo )
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

                for( int icolor = 0; icolor < pending._count.color_rt; ++icolor )
                {
                    const bxGdiTexture rt = pending._color_rt[icolor];
                    for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
                    {
                        const unsigned stage_mask = BIT_OFFSET( istage );
                        for( int islot = 0; islot < cMAX_TEXTURES; ++islot )
                        {
                            const bxGdiTexture tex = current._textures[istage][islot];
                            found_stages |= ( ( tex.id == rt.id ) || ( tex.id == pending._depth_rt.id ) ) * stage_mask;
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
				ctx->changeRenderTargets((bxGdiTexture*)pending._color_rt, pending._count.color_rt, pending._depth_rt);
            }
        }

        if( diff.viewport )
        {
			ctx->setViewport( pending._viewport );
        }

        if( diff.clearColor )
        {
            const ClearColorData& ccd = pending._clear_color;
            ctxBackend->clearBuffers( (bxGdiTexture*)pending._color_rt, pending._count.color_rt, pending._depth_rt, (float*)ccd.rgbad, ccd.flag_clearColor, ccd.flag_clearDepth );
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
    void prepare_draw( bxGdiContext* ctx )
    {
        const StateDiff sdiff = _StateDiff( ctx->current, ctx->pending );
        const StateInfo sinfo = state_info( ctx->pending );

        _SubmitState( ctx, ctx->current, ctx->pending, sdiff, sinfo );  

        if( ctx->pending._topology != ctx->current._topology )
        {
            ctx->_ctx->setTopology( ctx->pending._topology );
        }

        ctx->pending._clear_color.reset();
        ctx->current = ctx->pending;
    }
    ///
    ///
}

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

}

void bxGdiContext::setViewport(const bxGdiViewport& vp)
{
}

void bxGdiContext::setVertexBuffers(bxGdiVertexBuffer* vbuffers, unsigned n)
{
}

void bxGdiContext::setIndexBuffer(bxGdiIndexBuffer ibuffer)
{
}

void bxGdiContext::setShaders(bxGdiShader* shaders, int n, unsigned vertex_input_mask)
{
}

void bxGdiContext::setCbuffers(bxGdiBuffer* cbuffers, unsigned start_slot, unsigned n, int stage)
{
}

void bxGdiContext::setTextures(bxGdiTexture* textures, unsigned start_slot, unsigned n, int stage)
{
}

void bxGdiContext::setSamplers(bxGdiSamplerDesc* samplers, unsigned start_slot, unsigned n, int stage)
{
}

void bxGdiContext::setCbuffer(bxGdiBuffer cbuffer, int slot, unsigned stage_mask)
{
}

void bxGdiContext::setTexture(bxGdiTexture texture, int slot, unsigned stage_mask)
{
}

void bxGdiContext::setSampler(const bxGdiSamplerDesc& sampler, int slot, unsigned stage_mask)
{
}

void bxGdiContext::setHwState(const bxGdiHwStateDesc& hwstate)
{
}

void bxGdiContext::setTopology(int topology)
{
}

void bxGdiContext::clearTextures()
{
}

void bxGdiContext::clearSamplers()
{
}

void bxGdiContext::changeToMainFramebuffer()
{
}

void bxGdiContext::changeRenderTargets(bxGdiTexture* color_rts, unsigned n_rt, bxGdiTexture depth_rt)
{
}

void bxGdiContext::clearBuffers(float rgbad[5], int flag_color, int flag_depth)
{
}

void bxGdiContext::clearBuffers(float r, float g, float b, float a, float d, int flag_color, int flag_depth)
{
}

void bxGdiContext::draw(unsigned num_vertices, unsigned start_index)
{
}

void bxGdiContext::drawIndexed(unsigned num_indices, unsigned start_index, unsigned base_vertex)
{
}

void bxGdiContext::drawInstanced(unsigned num_vertices, unsigned start_index, unsigned num_instances)
{
}

void bxGdiContext::drawIndexedInstanced(unsigned num_indices, unsigned start_index, unsigned num_instances, unsigned base_vertex)
{
}

///
///
void bxGdiDrawCall::setVertexBuffers(bxGdiVertexBuffer* buffers, int nBuffers)
{
}

void bxGdiDrawCall::setIndexBuffer(bxGdiIndexBuffer ibuffer)
{
}

void bxGdiDrawCall::setShaders(bxGdiShader* shaders, int n)
{
}

void bxGdiDrawCall::setCbuffers(bxGdiBuffer* cbuffers, unsigned startSlot, unsigned n, int stage)
{
}

void bxGdiDrawCall::setTextures(bxGdiTexture* textures, unsigned startSlot, unsigned n, int stage)
{
}

void bxGdiDrawCall::setSamplers(bxGdiSamplerDesc* samplers, unsigned startSlot, unsigned n, int stage)
{
}

void bxGdiDrawCall::setHwState(const bxGdiHwStateDesc& hwState)
{
}
///
///
bxGdiDrawCall* bxGdiCommandBuffer::newDrawCall()
{
}

void bxGdiCommandBuffer::submitDrawCall(const bxGdiDrawCall* dcall, const void* key)
{
}

void bxGdiCommandBuffer::sort()
{
}

void bxGdiCommandBuffer::flush(bxGdiContext* ctx)
{
}