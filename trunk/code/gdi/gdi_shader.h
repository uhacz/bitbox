#pragma once

#include "gdi_backend.h"

struct bxGdiShaderPass
{
    bxGdiShader progs[bxGdi::eDRAW_STAGES_COUNT];
    bxGdiHwState hwState;
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
	        u8 stage_mask;
	        i8 pass_index;
	
	        const bool is_null() const { return index == -1; }

            ResourceDesc()
                : index(-1)
                , slot(0)
                , stage_mask(0)
                , pass_index(-1)
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
	        u8 stage_mask;

	        const bool is_null() const { return offset == 0xffff; }
	        static CBufferDesc null() { CBufferDesc sb = { 0xffff }; return sb; }
        };
        struct UniformDesc
        {
	        u16 offset;
            u16 size;
	        u16 buffer_index;
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
    bxGdiShaderFx* shaderFx();
    int findPass( const char* name );

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
	void setCBuffer( const char* name, const void* data, unsigned size );
	void setUniform( const char* name, const void* data, unsigned size );
    template< class T > void setUniform( const char* name, const T& value )	{ setUniform( shi, name, &value, sizeof(T) ); }

    const bxGdiHwState hwState( int passIndex ) const;
    void setHwState( int passIndex, const bxGdiHwState& hwstate );

    ///
    ///
    u16 vertexInputMask( int passIndex );
    const bxGdiShader* programs( int passIndex ) const;
    
    ///
    ///
    u32 sortHash() const { return _sortHash; }

private:
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
namespace bxGdi
{
    int shaderFx_initParams( bxGdiShaderFx* fx, const ShaderReflection& reflection, const char* materialCBufferName = "MaterialData" );
    void shaderFx_deinitParams( bxGdiShaderFx* fx );

    int shaderFx_createFromFile( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager, bxGdiShaderFx* fx, const char* fileNameWithoutExt );
    void shaderFx_release( bxGdiDeviceBackend* dev, bxGdiShaderFx* fx );

    int shaderFx_createInstance( bxGdiShaderFx_Instance* fxInstance, bxGdiShaderFx* fx );
    void shaderFx_releaseInstance( bxGdiShaderFx_Instance* fxInstance );

}///