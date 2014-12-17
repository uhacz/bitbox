#include "gdi_shader.h"
#include "gdi_shader_reflection.h"
#include "gdi_context.h"
#include <util/hash.h>
#include <util/array_util.h>
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
    int shaderFx_findPass( const bxGdiShaderFx* fx, const char* passName )
    {
        const u32 passHashedName = simple_hash( passName );
        const int index = array::find1( fx->_passHashedNames, fx->_passHashedNames + fx->_numPasses, array::OpEqual<u32>( passHashedName ) );
        if( index == -1 )
        {
            bxLogError( "Pass '%s' not found", passName );
        }
        return index;
    }
    const bxGdiShaderFx::TextureDesc* shaderFx_findTexture( const bxGdiShaderFx* fx, u32 hashedName, int startIndex )
    {
        const u32* begin = fx->_hashedNames + fx->_nameTextures_begin + startIndex;
        const u32* end = fx->_hashedNames + fx->_nameTextures_begin + fx->_numTextures;
        
        const int index = array::find1( begin, end, array::OpEqual<u32>( hashedName ) );
        return ( index == -1 ) ? 0 : fx->_textures + index + startIndex;
    }

    const bxGdiShaderFx::SamplerDesc* shaderFx_findSampler( const bxGdiShaderFx* fx, u32 hashedName, int startIndex )
    {
        const u32* begin = fx->_hashedNames + fx->_nameSamplers_begin + startIndex;
        const u32* end = fx->_hashedNames + fx->_nameSamplers_begin + fx->_numSamplers;
        
        const int index = array::find1( begin, end, array::OpEqual<u32>( hashedName ) );
        return ( index == -1 ) ? 0 : fx->_samplers + index + startIndex;
        
    }
    const bxGdiShaderFx::UniformDesc* shaderFx_findUniform( const bxGdiShaderFx* fx, u32 hashedName )
    {
        const u32* begin = fx->_hashedNames + fx->_nameUniforms_begin;
        const u32* end = begin + fx->_numUniforms;
        
        int index = array::find1( begin, end, array::OpEqual<u32>( hashedName ) );
        return ( index == -1 ) ? 0 : fx->_uniforms + index;
    }
}

void bxGdiShaderFx_Instance::setTexture( const char* name, bxGdiTexture tex )
{
    bxGdiShaderFx* fx = _fx;
    int done = 0;
    int index = 0;
    const u32 hashedName = simple_hash( name );
    do
    {
        const bxGdiShaderFx::TextureDesc* param = bxGdi::shaderFx_findTexture( fx, hashedName, index );
        if( !param )
            break;

        _textures[param->index] = tex;
        _flag_texturesDirty |= 1;

        index = param->index + 1;
    } while( !done );
}
void bxGdiShaderFx_Instance::setSampler( const char* name, const bxGdiSamplerDesc& sampler )
{
    bxGdiShaderFx* fx = _fx;
    int done = 0;
    int index = 0;
    const u32 hashedName = simple_hash( name );
    do
    {
        const bxGdiShaderFx::SamplerDesc* param = bxGdi::shaderFx_findSampler( fx, hashedName, index );
        if( !param )
            break;

        _samplers[param->index] = sampler;
        index = param->index + 1;
    } while( !done );
}

void bxGdiShaderFx_Instance::setUniform( const char* name, const void* data, unsigned size )
{
    const u32 hashedName = simple_hash( name );
    const bxGdiShaderFx::UniformDesc* desc = bxGdi::shaderFx_findUniform( _fx, hashedName );
    if( !desc )
        return;

    const u32 s = ( size < desc->size ) ? size : desc->size;
    const u32 offset = desc->offset;
    SYS_ASSERT( offset + size <= _sizeDataCBuffers );

	u8* dst = _dataCBuffers + offset;
	memcpy( dst, data, size );

    _SetBufferDirty( desc->bufferIndex );
}

void bxGdiShaderFx_Instance::uploadCBuffers(bxGdiContextBackend* ctx)
{
    for( int ibuffer = 0; ibuffer < numCBuffers(); ++ibuffer )
    {
        if ( !isBufferDirty( ibuffer ) )
            continue;

        bxGdiBuffer cbuffer = cbuffers()[ibuffer];

        const bxGdiShaderFx::CBufferDesc desc = cbufferDescs()[ibuffer];
        const u8* data = _dataCBuffers + desc.offset;

        ctx->updateCBuffer( cbuffer, data );
        _MarkBufferAsClean( ibuffer );
    }
}


namespace bxGdi
{
    int _ShaderFx_initParams( bxGdiShaderFx* fx, const ShaderReflection& reflection, const char* materialCBufferName )
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
            param.passIndex = desc.pass_index;
		    param.stageMask = desc.stage_mask;
        }
        for( u32 i = 0; i < reflection.samplers.size(); ++i, ++paramIndex )
        {
            const ShaderSamplerDesc& desc = reflection.samplers[i];
            paramsHashedNames[paramIndex] = desc.hashed_name;

            bxGdiShaderFx::SamplerDesc& param = fx->_samplers[i];
            param.index = i;
            param.slot = desc.slot;
            param.passIndex = desc.pass_index;
		    param.stageMask = desc.stage_mask;
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
		    param.stageMask = (u8)desc.stage_mask;
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
			    param.bufferIndex = (u16)i;
            }

            offset += (u16)cbdesc.size;
        }

        return 0;
    }
    void _ShaderFx_deinitParams( bxGdiShaderFx* fx )
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

    bxGdiShaderFx* shaderFx_createFromFile( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* fileNameWithoutExt, bxAllocator* allocator )
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
            return 0;
        }
        
        using namespace fxTool;

        const char* shaderTxt = shaderFile.txt;
        const int shaderTxtLen = (int)shaderFile.size;
        FxSourceDesc fxSrc;
        int ierr = parse_header( &fxSrc, shaderTxt, shaderTxtLen );
        if( ierr != 0 )
        {
            return 0;
        }
    
        const unsigned numPasses = (unsigned)fxSrc.passes.size();
        u32 memSize = 0;
        memSize += sizeof( bxGdiShaderFx );
        memSize += numPasses * sizeof(bxGdiShaderPass);
        memSize += numPasses * sizeof(u32);

        void* mem = BX_MALLOC( allocator, memSize, sizeof( void* ) );
        memset( mem, 0, memSize );

        bxBufferChunker chunker( mem, memSize );
        bxGdiShaderFx* fx = chunker.add< bxGdiShaderFx >();
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
                if( found == array::end( global_reflection.cbuffers ) )
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

        _ShaderFx_initParams( fx, global_reflection );
        shaderFile.release();
        fxTool::release( &fxSrc );
        return fx;
    }
    void shaderFx_release( bxGdiDeviceBackend* dev, bxGdiShaderFx** fxPtr, bxAllocator* allocator )
    {
        if( !fxPtr[0] )
        {
            return;
        }


        bxGdiShaderFx* fx = fxPtr[0];
        SYS_ASSERT( fx->_numInstances == 0 );
        _ShaderFx_deinitParams( fx );

        for( int ipass = 0; ipass < fx->_numPasses; ++ipass )
        {
            bxGdiShaderPass& pass = fx->_passes[ipass];
            for( int istage = 0; istage < bxGdi::eDRAW_STAGES_COUNT; ++istage )
            {
                dev->releaseShader( &pass.progs[istage] );
            }
        }

        BX_FREE0( allocator, fx );
        fxPtr[0] = 0;
    }

    int _ShaderFx_calculateBufferMemorySize( bxGdiShaderFx* fx )
    {
        int size = 0;
        const int n = fx->_numCBuffers;
        for( int i = 0; i < n; ++i )
        {
            size += fx->_cbuffers[i].size;
        }
        return size;
    }
    
    bxGdiShaderFx_Instance* shaderFx_createInstance( bxGdiDeviceBackend* dev, bxGdiShaderFx* fx, bxAllocator* allocator )
    {
        const int cbuffersSize = _ShaderFx_calculateBufferMemorySize( fx );

	    u32 memSize = 0;
        memSize += sizeof( bxGdiShaderFx_Instance );
	    memSize += fx->_numTextures * sizeof(bxGdiTexture);
	    memSize += fx->_numSamplers * sizeof(bxGdiSamplerDesc);
	    memSize += fx->_numCBuffers * sizeof(bxGdiBuffer);
	    memSize += cbuffersSize;

        if( !memSize )
            return 0;

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, sizeof(void*) ); // util::default_allocator()->allocate( mem_size, sizeof( void* ) );
        memset( mem, 0, memSize );

        bxBufferChunker chunker( mem, memSize )
            ;
        bxGdiShaderFx_Instance* fxInstance = chunker.add< bxGdiShaderFx_Instance >();
        fxInstance->_textures = chunker.add<bxGdiTexture>( fx->_numTextures );
        fxInstance->_samplers = chunker.add<bxGdiSamplerDesc>( fx->_numSamplers );
        fxInstance->_cbuffers = chunker.add<bxGdiBuffer>( fx->_numCBuffers );
        fxInstance->_dataCBuffers = chunker.add<u8>( cbuffersSize );
        fxInstance->_sizeDataCBuffers = cbuffersSize;
        chunker.check();

        fxInstance->_sortHash = 0;
        fxInstance->_flag_cbuffeDirty = 0;
        fxInstance->_flag_texturesDirty = 0;

        for( int i = 0; i < fx->_numCBuffers; ++i )
        {
            const bxGdiShaderFx::CBufferDesc& desc = fx->_cbuffers[i];
            fxInstance->_cbuffers[i] = dev->createConstantBuffer( desc.size );
        }

        fxInstance->_fx = fx;
        ++fx->_numInstances;

        return fxInstance;
    }
    void shaderFx_releaseInstance( bxGdiDeviceBackend* dev, bxGdiShaderFx_Instance** fxInstancePtr, bxAllocator* allocator )
    {
        if( !fxInstancePtr[0] )
            return;

        bxGdiShaderFx_Instance* fxInstance = fxInstancePtr[0];
        bxGdiShaderFx* fx = fxInstance->shaderFx();
        SYS_ASSERT( fx->_numInstances > 0 );
        --fx->_numInstances;

        for( int i = 0; i < fx->_numCBuffers; ++i )
        {
            dev->releaseBuffer( &fxInstance->_cbuffers[i] );
        }

        BX_FREE( allocator, fxInstance );

        fxInstancePtr[0] = 0;
    }

    bxGdiShaderFx_Instance* shaderFx_createWithInstance(bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* fileNameWithoutExt, bxAllocator* allocator)
    {
        bxGdiShaderFx* fx = shaderFx_createFromFile( dev, resourceManager, fileNameWithoutExt, allocator );
        if ( !fx )
            return 0;

        return shaderFx_createInstance( dev, fx, allocator );
    }

    void shaderFx_releaseWithInstance(bxGdiDeviceBackend* dev, bxGdiShaderFx_Instance** fxInstance, bxAllocator* allocator)
    {
        bxGdiShaderFx* fx = fxInstance[0]->shaderFx();
        shaderFx_releaseInstance( dev, fxInstance, allocator );
        shaderFx_release( dev, &fx, allocator );
    }

    void shaderFx_enable( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName )
    {
        int passIndex = shaderFx_findPass( fxI->shaderFx(), passName );
        if( passIndex == -1 )
        {
            return;
        }

        shaderFx_enable( ctx, fxI, passIndex );
    }

    void shaderFx_enable( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, int passIndex )
    {
        ctx->setShaders( (bxGdiShader*)fxI->programs( passIndex ), bxGdi::eDRAW_STAGES_COUNT, fxI->vertexInputMask( passIndex ) );
        ctx->setHwState( fxI->hwState( passIndex ) );

        {
            fxI->uploadCBuffers( ctx->backend() );
            const bxGdiShaderFx::CBufferDesc* descs = fxI->cbufferDescs();
            const bxGdiBuffer* resources = fxI->cbuffers();
            const int nResources = fxI->numCBuffers();
            for( int iresource = 0; iresource < nResources; ++iresource )
            {
                ctx->setCbuffer( resources[iresource], descs[iresource].slot, descs[iresource].stageMask );
            }
        }

        {
            const bxGdiShaderFx::TextureDesc* descs = fxI->textureDescs();
            const bxGdiTexture* resources = fxI->textures();
            const int nResources = fxI->numTextures();
            for ( int iresource = 0; iresource < nResources; ++iresource )
            {
                if ( descs[iresource].passIndex == passIndex )
                     ctx->setTexture( resources[iresource], descs[iresource].slot, descs[iresource].stageMask );
            }
        }

        {
            const bxGdiShaderFx::SamplerDesc* descs = fxI->samplerDescs();
            const bxGdiSamplerDesc* resources = fxI->samplers();
            const int nResources = fxI->numSamplers();
            for ( int iresource = 0; iresource < nResources; ++iresource )
            {
                if ( descs[iresource].passIndex == passIndex )
                    ctx->setSampler( resources[iresource], descs[iresource].slot, descs[iresource].stageMask );
            }
        }
     
    }



}///