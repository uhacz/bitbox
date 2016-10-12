#pragma once

#include <util/type.h>
#include <util/vector.h>

namespace bx{ namespace rdi{

struct ShaderVariableDesc
{
    ShaderVariableDesc() : hashed_name( hashed_name ), offset( 0 ), size( 0 ), type( 0 ) {}
    u32 hashed_name;
    u32 offset;
    u32 size;
    u32 type;
};

struct ShaderCBufferDesc
{
    ShaderCBufferDesc() : hashed_name( 0 ), size( 0 ), slot( 0 ), stage_mask( 0 ) {}
    u32 hashed_name;
    u32 size;
    u8 slot;
    u8 stage_mask;
    u8 __padd[2];
    vector_t<ShaderVariableDesc> variables;
};

struct ShaderTextureDesc
{
    ShaderTextureDesc() : hashed_name( 0 ), slot( 0 ), dimm( 0 ), is_cubemap( 0 ), pass_index( -1 ) {}
    u32 hashed_name;
    u8 slot;
    u8 stage_mask;
    u8 dimm;
    u8 is_cubemap;
    i8 pass_index;
};

struct ShaderSamplerDesc
{
    ShaderSamplerDesc() : hashed_name( 0 ), slot( 0 ), pass_index( -1 ) {}
    u32 hashed_name;
    u8 slot;
    u8 stage_mask;
    i8 pass_index;
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