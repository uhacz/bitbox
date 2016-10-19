#pragma once

#include <util/type.h>
#include <util/vector.h>
#include <string>

namespace bx{ namespace rdi{

struct ShaderVariableDesc
{
    std::string name;
    u32 hashed_name = 0;
    u32 offset = 0;
    u32 size = 0;
    u32 type = 0;
};

struct ShaderCBufferDesc
{
    std::string name;
    u32 hashed_name = 0;
    u32 size = 0;
    u8 slot = 0;
    u8 stage_mask = 0;
    u8 __padd[2] = {};
    vector_t<ShaderVariableDesc> variables;
};

struct ShaderTextureDesc
{
    ShaderTextureDesc() : hashed_name( 0 ), slot( 0 ), dimm( 0 ), is_cubemap( 0 ), pass_index( -1 ) {}
    std::string name;
    u32 hashed_name = 0;
    u8 slot = 0;
    u8 stage_mask = 0;
    u8 dimm = 0;
    u8 is_cubemap = 0;
    i8 pass_index = -1;
};

struct ShaderSamplerDesc
{
    std::string name;
    u32 hashed_name = 0;
    u8 slot = 0;
    u8 stage_mask = 0;
    i8 pass_index = -1;
};

struct ShaderReflection
{
    ShaderReflection() {}

    vector_t<ShaderCBufferDesc> cbuffers;
    vector_t<ShaderTextureDesc> textures;
    vector_t<ShaderSamplerDesc> samplers;
    VertexLayout vertex_layout;
    u16 input_mask;
    u16 __pad0[1];

    //////////////////////////////////////////////////////////////////////////
    /// searching stuff
    struct FunctorCBuffer{
        u32 _hashed_name;

        explicit FunctorCBuffer( u32 hn ) : _hashed_name( hn ) {}
        bool operator() ( const ShaderCBufferDesc& cbuffer ) const { return _hashed_name == cbuffer.hashed_name; }
    };
    struct FunctorTexture{
        u32 _hashed_name;

        explicit FunctorTexture( u32 hn ) : _hashed_name( hn ) {}
        bool operator() ( const ShaderTextureDesc& tex ) const { return _hashed_name == tex.hashed_name; }
    };
    struct FunctorSampler{
        u32 _hashed_name;

        explicit FunctorSampler( u32 hn ) : _hashed_name( hn ) {}
        bool operator() ( const ShaderSamplerDesc& sam ) const { return _hashed_name == sam.hashed_name; }
    };
};


}}///