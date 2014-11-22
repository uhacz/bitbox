#pragma once

#include "gdi_backend.h"
#include <util/memory.h>

struct bxGdiShaderPass
{
    bxGdiShader progs[bxGdi::eDRAW_STAGES_COUNT];
    bxGdiHwStateDesc hwState;
    u16 vertexInputMask;
    u16 padding0__[1];

    bxGdiShaderPass()
        : vertexInputMask(0)
    {}
};

struct bxGdiShaderFx
{
        struct ResourceDesc
        {
            i8 index;
	        u8 slot;
	        u8 stageMask;
	        i8 passIndex;
	
	        const bool is_null() const { return index == -1; }

            ResourceDesc()
                : index(-1)
                , slot(0)
                , stageMask(0)
                , passIndex(-1)
            {}
        };

        struct TextureDesc : public ResourceDesc
        {};
        struct SamplerDesc : public ResourceDesc
        {};
 
        struct CBufferDesc
        {
            u16 offset;
            u16 size;
	        i16 index;
	        u8 slot;
	        u8 stageMask;

	        const bool is_null() const { return offset == 0xffff; }
	        static CBufferDesc null() { CBufferDesc sb = { 0xffff }; return sb; }
        };
        struct UniformDesc
        {
	        u16 offset;
            u16 size;
	        u16 bufferIndex;
	        u16 index;

	        const bool is_null() const { return offset == 0xffff; }
	        static UniformDesc null() { UniformDesc su = { 0xffff }; return su; }
        };  
    ///
    ///
    struct TextureDesc;
    struct SamplerDesc;
    struct CBufferDesc;
    struct UniformDesc;

    ///
    ///
    bxGdiShaderPass* _passes;
    u32* _passHashedNames;
    i32 _numPasses;
    i32 _numInstances;

    ///
    ///
    u32* _hashedNames;
	TextureDesc* _textures;
    SamplerDesc* _samplers;
    CBufferDesc* _cbuffers;
    UniformDesc* _uniforms;

	u8 _nameTextures_begin;
	u8 _nameSamplers_begin;
	u8 _nameCBuffers_begin;
	u8 _nameUniforms_begin;

    u8 _numTextures;
    u8 _numSamplers;
    u8 _numCBuffers;
    u8 _numUniforms;

    bxGdiShaderFx();
    ~bxGdiShaderFx();
};

struct bxGdiShaderFx_Instance
{
    bxGdiShaderFx_Instance();
    ~bxGdiShaderFx_Instance();

    ///
    ///
    bxGdiShaderFx* shaderFx() { return _fx; }

    ///
    ///
    int numTextures() const { return _fx->_numTextures; }
    int numSamplers() const { return _fx->_numSamplers; }
    int numCBuffers() const { return _fx->_numCBuffers; }

    const bxGdiShaderFx::TextureDesc* textureDescs() const { return _fx->_textures; }
    const bxGdiShaderFx::SamplerDesc* samplerDescs() const { return _fx->_samplers; }
    const bxGdiShaderFx::CBufferDesc* cbufferDescs() const { return _fx->_cbuffers; }

    const bxGdiTexture*     textures() const { return _textures; }
    const bxGdiSamplerDesc* samplers() const { return _samplers; }
    const bxGdiBuffer*      cbuffers() const { return _cbuffers; }

    ///
    ///
    void setTexture( const char* name, bxGdiTexture tex );
	void setSampler( const char* name, const bxGdiSamplerDesc& sampler );
	void setUniform( const char* name, const void* data, unsigned size );
    template< class T > void setUniform( const char* name, const T& value )	{ setUniform( shi, name, &value, sizeof(T) ); }

    const bxGdiHwStateDesc hwState( int passIndex ) const { return _fx->_passes[passIndex].hwState; }
    void setHwState( int passIndex, const bxGdiHwStateDesc& hwstate ) { _fx->_passes[passIndex].hwState = hwstate;  }

    ///
    ///
    u16                vertexInputMask( int passIndex ) const { return _fx->_passes[passIndex].vertexInputMask; }
    const bxGdiShader* programs       ( int passIndex ) const { return _fx->_passes[passIndex].progs;  }
    
    void uploadCBuffers( bxGdiContextBackend* ctx );

    ///
    ///
    u32 sortHash() const { return _sortHash; }

    int isBufferDirty( int index ) const 
    {
        SYS_ASSERT( index < 32 );
	    return _flag_cbuffeDirty & ( 1 << index );
    }
    void _SetBufferDirty( int index )
    {
        SYS_ASSERT( index < 32 );
	    _flag_cbuffeDirty |= ( 1 << index );
    }
    void _MarkBufferAsClean( int index )
    {
        SYS_ASSERT( index < 32 );
        _flag_cbuffeDirty &= ~( 1 << index );
    }

public:
    bxGdiShaderFx*    _fx; // weakRef
    bxGdiTexture*     _textures;
    bxGdiSamplerDesc* _samplers;
    bxGdiBuffer*      _cbuffers;
    u8*				  _dataCBuffers;
	u32				  _sizeDataCBuffers;
	u32               _sortHash;
    u32				  _flag_cbuffeDirty : 1;
    u32               _flag_texturesDirty : 1;
};


class bxResourceManager;
struct bxGdiContext;
class bxGdiDrawCall;
namespace bxGdi
{
    int  _ShaderFx_initParams( bxGdiShaderFx* fx, const ShaderReflection& reflection, const char* materialCBufferName = "MaterialData" );
    void _ShaderFx_deinitParams( bxGdiShaderFx* fx );
    
    bxGdiShaderFx* shaderFx_createFromFile( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* fileNameWithoutExt, bxAllocator* allocator = bxDefaultAllocator() );
    bxGdiShaderFx_Instance* shaderFx_createInstance( bxGdiDeviceBackend* dev, bxGdiShaderFx* fx, bxAllocator* allocator = bxDefaultAllocator() );
    void shaderFx_release( bxGdiDeviceBackend* dev, bxGdiShaderFx** fx, bxAllocator* allocator = bxDefaultAllocator() );
    void shaderFx_releaseInstance( bxGdiDeviceBackend* dev, bxGdiShaderFx_Instance** fxInstance, bxAllocator* allocator = bxDefaultAllocator() );

    bxGdiShaderFx_Instance* shaderFx_createWithInstance( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, const char* fileNameWithoutExt, bxAllocator* allocator = bxDefaultAllocator() );
    void shaderFx_releaseWithInstance( bxGdiDeviceBackend* dev, bxGdiShaderFx_Instance** fxInstance, bxAllocator* allocator = bxDefaultAllocator() );
    
    int shaderFx_findPass( const bxGdiShaderFx* fx, const char* passName );
    const bxGdiShaderFx::TextureDesc* shaderFx_findTexture( const bxGdiShaderFx* fx, u32 hashedName, int startIndex );
    const bxGdiShaderFx::SamplerDesc* shaderFx_findSampler( const bxGdiShaderFx* fx, u32 hashedName, int startIndex );
    const bxGdiShaderFx::UniformDesc* shaderFx_findUniform( const bxGdiShaderFx* fx, u32 hashedName );


    void shaderFx_enable( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, const char* passName );
    void shaderFx_enable( bxGdiContext* ctx, bxGdiShaderFx_Instance* fxI, int passIndex );
    void shaderFx_enable( bxGdiDrawCall* dcall, bxGdiShaderFx_Instance, const char* passName );
    void shaderFx_enable( bxGdiDrawCall* dcall, bxGdiShaderFx_Instance, int passIndex );
}///