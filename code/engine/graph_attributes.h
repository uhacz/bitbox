#pragma once

#include <util/containers.h>
#include <util/vectormath/vectormath.h>

namespace bx
{
    typedef i32 AttributeIndex;
    
    namespace AttributeType
    {
        enum Enum : u8
        {
            eINT = 0,
            eFLOAT,
            eFLOAT3,
            eSTRING,

            eTYPE_COUNT,
        };
        static const unsigned char _stride[eTYPE_COUNT] =
        {
            sizeof( i32 ),
            sizeof( f32 ),
            sizeof( f32 ) * 3,
            sizeof( void* ),
        };
        static const unsigned char _alignment[eTYPE_COUNT] =
        {
            4,
            4,
            4,
            8,
        };

        struct String
        {
            char* c_str;
        };
    }///

    struct AttributeStruct
    {
        union Name
        {
            u64 hash = 0;
            char str[8];
        };

        void* _memory_handle = nullptr;
        i32 _size = 0;
        i32 _capacity = 0;

        AttributeType::Enum* _type = nullptr;
        Name* _name = nullptr;
        u16* _offset = nullptr;

        array_t< u8 > _default_values;

        int find( const char* name ) const;
        void alloc( int newCapacity );
        void free();
        void setDefaultValue( int index, const void* defaulValue );
        void setDefaultValueString( int index, const char* defaultString );
        int add( const char* name, AttributeType::Enum typ );

        AttributeType::Enum type( int index ) const { return _type[index]; }
        int stride( int index ) const { return AttributeType::_stride[_type[index]]; }
        int alignment( int index ) const { return AttributeType::_alignment[_type[index]]; }
    };

    struct AttributeInstance
    {
        struct Value
        {
            u16 type = AttributeType::eTYPE_COUNT;
            u16 offset = 0;
        };

        Value* _values = nullptr;
        u8* _blob = nullptr;
        u32 _blob_size_in_bytes = 0;
        i32 _num_values = 0;

        static void startup( AttributeInstance** outPtr, const AttributeStruct& attrStruct );
        static void shutdown( AttributeInstance** attrIPtr );

        u8* pointerGet( int index, AttributeType::Enum type );
        void dataSet( int index, AttributeType::Enum type, const void* data );
        void stringSet( int index, const char* str );

    };
}///
