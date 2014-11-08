#include "gdi_shader.h"
#include "gdi_shader_reflection.h"

#include <util/memory.h>
#include <util/hash.h>
#include <util/buffer_utils.h>
#include <resource_manager/resource_manager.h>
#include <tools/fx_compiler/fx_compiler.h>

#include <memory.h>

bxGdiShaderFx::bxGdiShaderFx()
{
    memset( this, 0, sizeof( *this ) );
}
bxGdiShaderFx::~bxGdiShaderFx()
{
    SYS_ASSERT( _passes == 0 );
}

bxGdiShaderFx_Instance::bxGdiShaderFx_Instance()
{
    memset( this, 0, sizeof( *this ) );
}
bxGdiShaderFx_Instance::~bxGdiShaderFx_Instance()
{
    SYS_ASSERT( _textures == 0 );
}

namespace bxGdi
{
    int shaderFx_initParams( bxGdiShaderFx* fx, const ShaderReflection& reflection, const char* materialCBufferName )
    {
        SYS_ASSERT( fx->_hashedNames == 0 );

        const int MAX_BUFFERS = bxGdi::cMAX_CBUFFERS;
        const u32 cbufferHashedName = simple_hash( materialCBufferName );
        
        u32 validBufferIndices[MAX_BUFFERS];
    
        const u32 numTextures = array::size( reflection.textures );
        const u32 numSamplers = array::size( reflection.samplers );
        u32 numCBuffers = 0;
	    for ( u32 i = 0; i < (u32)reflection.cbuffers.size(); ++i )
        {
            if( reflection.cbuffers[i].hashed_name == cbufferHashedName )
            {
                validBufferIndices[numCBuffers++] = i;
                break;
            }
        }

	    u32 numUniforms = 0;
        for ( u32 i = 0; i < numCBuffers; ++i )
        {
            const ShaderCBufferDesc& desc = reflection.cbuffers[validBufferIndices[i]];
		    numUniforms += (u32)desc.variables.size();
        }
	
	    const u32 numParams = numTextures + numSamplers + numCBuffers + numUniforms;

        u32 memSize = 0;
        memSize += numParams * sizeof(u32);
	    memSize += numTextures * sizeof( bxGdiShaderFx::TextureDesc );
        memSize += numSamplers * sizeof( bxGdiShaderFx::SamplerDesc );
        memSize += numCBuffers * sizeof( bxGdiShaderFx::CBufferDesc );
	    memSize += numUniforms * sizeof( bxGdiShaderFx::UniformDesc );
    
        if( !memSize )
            return false;

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 8 );
        memset( mem, 0, memSize );

        bxBufferChunker chunker( mem, memSize );

        fx->_hashedNames = chunker.add<u32>( numParams );
        fx->_textures = chunker.add< bxGdiShaderFx::TextureDesc >( numTextures );
        fx->_samplers = chunker.add< bxGdiShaderFx::SamplerDesc >( numSamplers );
        fx->_cbuffers = chunker.add< bxGdiShaderFx::CBufferDesc >( numCBuffers );
        fx->_uniforms = chunker.add< bxGdiShaderFx::UniformDesc >( numUniforms );
        chunker.check();

        fx->_numTextures = (u8)numTextures;
	    fx->_numSamplers = (u8)numSamplers;
	    fx->_numCBuffers = (u8)numCBuffers;
	    fx->_numUniforms = (u8)numUniforms;

	    fx->_nameTextures_begin = 0;
	    fx->_nameSamplers_begin = fx->_nameTextures_begin + numTextures;
	    fx->_nameCBuffers_begin = fx->_nameSamplers_begin + numSamplers;
	    fx->_nameUniforms_begin = fx->_nameCBuffers_begin + numCBuffers;

        u32* paramsHashedNames = fx->_hashedNames;

        u32 paramIndex = 0;

        for( u32 i = 0; i < reflection.textures.size(); ++i, ++paramIndex )
        {
            const ShaderTextureDesc& desc = reflection.textures[i];
            paramsHashedNames[paramIndex] = desc.hashed_name;
        
		    bxGdiShaderFx::TextureDesc& param = fx->_textures[i];
            param.index = i;
            param.slot = desc.slot;
            param.pass_index = desc.pass_index;
		    param.stage_mask = desc.stage_mask;
        }
        for( u32 i = 0; i < reflection.samplers.size(); ++i, ++paramIndex )
        {
            const ShaderSamplerDesc& desc = reflection.samplers[i];
            paramsHashedNames[paramIndex] = desc.hashed_name;

            bxGdiShaderFx::SamplerDesc& param = fx->_samplers[i];
            param.index = i;
            param.slot = desc.slot;
            param.pass_index = desc.pass_index;
		    param.stage_mask = desc.stage_mask;
        }

        u16 offset = 0;
        for( u32 i = 0; i < numCBuffers; ++i, ++paramIndex )
        {
            const ShaderCBufferDesc& desc = reflection.cbuffers[validBufferIndices[i]];
            paramsHashedNames[paramIndex] = desc.hashed_name;

		    bxGdiShaderFx::CBufferDesc& param = fx->_cbuffers[i];
            param.offset = offset;
            param.slot = desc.slot;
            param.size = desc.size;
		    param.stage_mask = (u8)desc.stage_mask;
		    param.index = i;
            offset += (u16)desc.size;
        }

        //SYS_ASSERT( (size_t)offset == size_buffers );

        offset = 0;
	    u32 uniform_index = 0;
	    for( u32 i = 0; i < numCBuffers; ++i )
        {
            const ShaderCBufferDesc& cbdesc = reflection.cbuffers[validBufferIndices[i]];
		    for( u32 j = 0; j < cbdesc.variables.size(); ++j, ++uniform_index, ++paramIndex )
            {
                const ShaderVariableDesc& desc = cbdesc.variables[j];
                paramsHashedNames[paramIndex] = desc.hashed_name;

			    bxGdiShaderFx::UniformDesc& param = fx->_uniforms[uniform_index];
                param.offset = offset + desc.offset;
                param.size = desc.size;
			    param.index = uniform_index;
			    param.buffer_index = (u16)i;
            }

            offset += (u16)cbdesc.size;
        }

        return 0;
    }
    void shaderFx_deinitParams( bxGdiShaderFx* fx )
    {
        BX_FREE( bxDefaultAllocator(), fx->_hashedNames );
        fx->_hashedNames = 0;
	    fx->_textures = 0;
        fx->_samplers = 0;
        fx->_cbuffers = 0;
        fx->_uniforms = 0;
	    fx->_nameTextures_begin = 0;
	    fx->_nameSamplers_begin = 0;
	    fx->_nameCBuffers_begin = 0;
	    fx->_nameUniforms_begin = 0;
        fx->_numTextures = 0;
        fx->_numSamplers = 0;
        fx->_numCBuffers = 0;
        fx->_numUniforms = 0;
    }

    int shaderFx_createFromFile( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGdiShaderFx* fx, const char* fileNameWithoutExt )
    {
        const char* shaderApiExt[] = 
        {
            "hlsl",
            "glsl",
        };
    
        const char* shaderApi = shaderApiExt[ 0 ];
        char fullFilename[256] = {0};
        sprintf_s( fullFilename, "shader/%s/%s.%s", shaderApi, fileNameWithoutExt, shaderApi );

        bxFS::File shaderFile = resourceManager->readTextFileSync( fullFilename );
        if( !shaderFile.ok() )
        {
            return -1;
        }
        
        using namespace fxTool;

        const char* shaderTxt = shaderFile.txt;
        const int shaderTxtLen = (int)shaderFile.size;
        FxSourceDesc fxSrc;
        int ierr = parse_header( &fxSrc, shaderTxt, shaderTxtLen );
        if( ierr != 0 )
        {
            return -1;
        }
    
        const unsigned numPasses = (unsigned)fxSrc.passes.size();
        u32 memSize = 0;
        memSize += numPasses * sizeof(bxGdiShaderPass);
        memSize += numPasses * sizeof(u32);

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, sizeof( void* ) );
        memset( mem, 0, memSize );

        bxBufferChunker chunker( mem, memSize );

        fx->_passes = chunker.add< bxGdiShaderPass >( numPasses );
        fx->_passHashedNames = chunker.add< u32 >( numPasses );
        chunker.check();

        fx->_numPasses = numPasses;

        ShaderReflection global_reflection;

        for( unsigned ipass = 0; ipass < numPasses; ++ipass )
        {
    	    const ConfigPass& cpass = fxSrc.passes[ipass];
    	    bxGdiShaderPass& pass = fx->_passes[ipass];
    	
            ShaderReflection local_reflection;

    	    fx->_passHashedNames[ipass] = simple_hash( cpass.name );
    	    for( int j = 0; j < bxGdi::eDRAW_STAGES_COUNT; ++j )
    	    {
    		    if( !cpass.entry_points[j] )
                {
                    pass.progs[j] = bxGdiShader();
                    continue;
                }

                char shaderBinFilename[256] = {0};
                sprintf_s( shaderBinFilename, "shader/%s/bin/%s.%s.%s", shaderApi, fileNameWithoutExt, cpass.name, bxGdi::stageName[j] );

                bxFS::File codeFile = resourceManager->readFileSync( shaderBinFilename );
                SYS_ASSERT( codeFile.ok() );
            
                bxGdiShader id_shader = dev->createShader( j, codeFile.bin, codeFile.size, &local_reflection );
                SYS_ASSERT( id_shader.id != 0 );

                pass.progs[j] = id_shader;

                if( j == bxGdi::eSTAGE_VERTEX )
                {
                    pass.vertexInputMask = local_reflection.input_mask;
                }
                codeFile.release();
    	    }

            for( int ibuffer = 0; ibuffer < array::size( local_reflection.cbuffers ); ++ibuffer )
            {
                const ShaderCBufferDesc& local_cbuffer = local_reflection.cbuffers[ibuffer];
                ShaderCBufferDesc* found = array::find( array::begin( global_reflection.cbuffers ), array::end( global_reflection.cbuffers ), ShaderReflection::FunctorCBuffer( local_cbuffer.hashed_name ) );
                if( !found )
                {
                    //index = (int)array::size( global_reflection.cbuffers );
                    global_reflection.cbuffers.push_back( local_cbuffer );
                    found = &array::back( global_reflection.cbuffers );
                }
                //SYS_ASSERT( global_reflection.cbuffers[index].slot == local_cbuffer.slot );
                SYS_ASSERT( found->slot == local_cbuffer.slot );
                found->stage_mask |= local_cbuffer.stage_mask;
            }
        
            for( int itexture = 0; itexture < array::size( local_reflection.textures ); ++itexture )
            {
                ShaderTextureDesc desc = local_reflection.textures[itexture];
                desc.pass_index = ipass;
                global_reflection.textures.push_back( desc );
            }

            for( int isampler = 0; isampler < array::size( local_reflection.samplers ); ++isampler )
            {
                ShaderSamplerDesc desc = local_reflection.samplers[isampler];
                desc.pass_index = ipass;
                global_reflection.samplers.push_back( desc );
            }

            //unsigned h = 1973;
            //progs.hash = util::elf_hash_data( (const u8*)progs.id, progs.n * sizeof( *progs.id ), h );
            pass.hwState = cpass.hwstate;
        }

        shaderFx_initParams( fx, global_reflection );

        fxTool::release( &fxSrc );
        return 0;
    }
    void shaderFx_release( bxGdiDeviceBackend* dev, bxGdiShaderFx* fx );

    int shaderFx_createInstance( bxGdiShaderFx_Instance* fxInstance, bxGdiShaderFx* fx );
    void shaderFx_releaseInstance( bxGdiShaderFx_Instance* fxInstance );

}///