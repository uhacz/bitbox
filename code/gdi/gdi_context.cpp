#include "gdi_context.h"
#include "gdi_backend.h"

namespace bxGdi
{
    StateDiff _StateDiff( const StateData& current, const StateData& pending )
    {
        StateDiff diff;

        diff.vstreams = memcmp( current._vstreams, pending._vstreams, sizeof(current._vstreams) );
        diff.vstreams |= current._count.vstreams != pending._count.vstreams;

        diff.vertexFormat = memcmp( current._vstreamBlocks, pending._vstreamBlocks, sizeof(current._vstreamBlocks) );
        diff.vertexFormat |= current._vertex_input_mask != pending._vertex_input_mask;

        diff.istream = current._istream.id != pending._istream.id;

        diff.shaders = memcmp( current._shaders, pending._shaders, sizeof(current._shaders) );
        diff.vertexFormat |= current._shaders[eSTAGE_VERTEX].id != pending._shaders[eSTAGE_VERTEX].id;

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
        diff.depthRT |= current._depth_rt != pending._depth_rt;
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

    void _SubmitState( bxGdiContextBackend* ctx, const StateData& current, const StateData& pending, StateDiff diff, StateInfo sinfo )
    {
        if( diff.colorRT || diff.depthRT )
        {
            if( pending._mainFramebuffer )
            {
                ctx->changeToMainFramebuffer();
            }
            else
            {
                unsigned found_stages = 0;

                for( int icolor = 0; icolor < pending._count.color_rt; ++icolor )
                {
                    const bxGdiTexture rt_id = pending._color_rt[icolor];
                    for( int istage = 0; istage < eSTAGE_COUNT; ++istage )
                    {
                        const unsigned stage_mask = BIT_OFFSET( istage );
                        for( int islot = 0; islot < cMAX_TEXTURES; ++islot )
                        {
                            const bxGdiTexture tex_id = current._textures[istage][islot];
                            found_stages |= ( ( tex_id == rt_id ) || ( tex_id == pending._depth_rt ) ) * stage_mask;
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
                            ctx->setTextures( 0, 0,
                            backend::set_textures( ctx, 0, 0, cMAX_TEXTURES, istage );
                            diff.textures |= stage_mask;
                        }
                    }
                }

                backend::change_render_targets( ctx, (bxGdiTexture*)pending._color_rt, pending._count.color_rt, pending._depth_rt );
            }

        }

        if( diff.viewport )
        {
            backend::set_viewport( ctx, pending._viewport.x, pending._viewport.y, pending._viewport.w, pending._viewport.h );
        }

        if( diff.clear_color )
        {
            const ClearColorData& ccd = pending._clear_color;
            backend::clear_buffers( ctx, (bxGdiTexture*)pending._color_rt, pending._count.color_rt, pending._depth_rt, (float*)ccd.rgbad, ccd.flag_clear_color, ccd.flag_clear_depth );
        }

        if( diff.shaders )
        {
            backend::set_shader_programs( ctx, (ShaderID*)pending._shader_programs, Pipeline::eSTAGE_COUNT );
        }

        if( diff.vstreams || diff.vertex_format )
        {
            VertexBufferID vbuffers[cMAX_VERTEX_BUFFERS];
            VertexStreamDesc descs[cMAX_VERTEX_BUFFERS];
            VertexStreamDesc null_desc;
            MEMZERO( &null_desc, sizeof( VertexStreamDesc ) );
            for( int i = 0; i < cMAX_VERTEX_BUFFERS; ++i )
            {
                const VertexBufferID vstream = pending._vstreams[i];
                const VertexStreamDesc desc = pending._vstream_descs[i];
                const unsigned stream_slot_mask = (vstream) ? vertex_stream_slot_mask( desc ) : 0;
                const unsigned bit = sinfo.active_vertex_slot_mask & stream_slot_mask;
                vbuffers[i] = (bit) ? vstream : 0;
                descs[i] = (bit) ?  desc : null_desc;
            }

            gdi::backend::set_vertex_buffers( ctx, vbuffers, descs, 0, cMAX_VERTEX_BUFFERS );
        }

        if( diff.istream )
        {
            gdi::backend::set_index_buffer( ctx, pending._istream, pending._istream_data_type );
        }

        if( diff.vertex_format )
        {
            gdi::VertexStreamDesc descs[cMAX_VERTEX_BUFFERS];
            int counter = 0;
            for( unsigned i = 0; i < pending._count.vstreams; ++i )
            {
                const gdi::VertexStreamDesc& d = pending._vstream_descs[i];
                int block_count = 0;
                for( int iblock = 0; iblock < d.count; ++iblock )
                {
                    gdi::VertexStreamDesc::Block block = d.blocks[iblock];
                    if( ( 1<<block.slot) & sinfo.active_vertex_slot_mask )
                    {
                        descs[counter].blocks[block_count++] = block;
                    }
                }
                SYS_ASSERT( block_count < gdi::VertexStreamDesc::eMAX_BLOCKS );
                descs[counter].count = block_count;
                counter += block_count > 0;
            }
            gdi::backend::set_vertex_format( ctx, descs, counter, pending._shader_programs[Pipeline::eSTAGE_VERTEX] );
        }

        if( diff.textures )
        {
            for( int istage = 0; istage < Pipeline::eSTAGE_COUNT; ++istage )
            {
                const u16 stage_mask = BIT_OFFSET( istage );
                if( ( stage_mask & diff.textures ) == 0 )
                    continue;

                backend::set_textures( ctx, (bxGdiTexture*)pending._textures[istage], 0, cMAX_TEXTURES, istage );
            }
        }

        if( diff.samplers )
        {
            for( int istage = 0; istage < Pipeline::eSTAGE_COUNT; ++istage )
            {
                const u16 stage_mask = BIT_OFFSET( istage );
                if( ( stage_mask & diff.samplers ) == 0 )
                    continue;

                backend::set_samplers( ctx, (SamplerState*)pending._samplers[istage], 0, cMAX_SAMPLERS, istage );
            }
        }

        if( diff.cbuffers )
        {
            for( int istage = 0; istage < Pipeline::eSTAGE_COUNT; ++istage )
            {
                const u16 stage_mask = BIT_OFFSET( istage );
                if( ( stage_mask & diff.cbuffers ) == 0 )
                    continue;

                backend::set_cbuffers( ctx, (BufferID*)pending._cbuffers[istage], 0, cMAX_CBUFFERS, istage );
            }
        }

        if( diff.hwstate_blend || diff.hwstate_depth || diff.hwstate_raster )
        {
            HwState hwstate;
            hwstate.blend.key = ( diff.hwstate_blend ) ? pending._hwstate.blend.key : 0;
            hwstate.depth.key = ( diff.hwstate_depth ) ? pending._hwstate.depth.key : 0;
            hwstate.raster.key= ( diff.hwstate_raster ) ? pending._hwstate.raster.key : 0;

            backend::set_hw_state( ctx, hwstate );
        }
    }

    void prepare_draw( CtxState* ctx )
    {
        const StateDiff sdiff = state_diff( ctx->current, ctx->pending );
        const StateInfo sinfo = state_info( ctx->pending );

        submit_state( ctx->backend, ctx->current, ctx->pending, sdiff, sinfo );  

        if( ctx->pending._topology != ctx->current._topology )
        {
            backend::set_topology( ctx->backend, ctx->pending._topology );
        }

        ctx->pending._clear_color.reset();
        ctx->current = ctx->pending;

    }
}