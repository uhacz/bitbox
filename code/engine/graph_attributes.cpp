#include "graph_context.h"
#include <string.h>
#include <util/buffer_utils.h>
#include <util/array.h>
#include <util/array_util.h>
#include <util/string_util.h>

namespace bx
{
    int AttributeStruct::find( const char* name ) const
    {
        Name name8;
        int i = 0;
        while( name[i] && i < 8 )
        {
            name8.str[i] = name[i];
            ++i;
        }
        //const Name* name8 = (Name*)name;
        for( int i = 0; i < _size; ++i )
        {
            if( name8.hash == _name[i].hash )
                return i;
        }
        return -1;
    }

    void AttributeStruct::alloc( int newCapacity )
    {
        int memSize = newCapacity * ( sizeof( Name ) + sizeof( AttributeType::Enum ) + sizeof( *_offset ) );
        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 4 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );
        AttributeType::Enum* newType = chunker.add< AttributeType::Enum >( newCapacity );
        Name* newName = chunker.add< Name >( newCapacity );
        u16* newOffset = chunker.add< u16 >( newCapacity );

        chunker.check();

        if( _size )
        {
            memcpy( newType, _type, _size * sizeof( *_type ) );
            memcpy( newName, _name, _size * sizeof( *_name ) );
            memcpy( newOffset, _offset, _size * sizeof( *_offset ) );
        }
        BX_FREE( bxDefaultAllocator(), _memory_handle );

        _memory_handle = mem;
        _type = newType;
        _name = newName;
        _offset = newOffset;
        _capacity = newCapacity;
    }

    void AttributeStruct::free()
    {
        for( int i = 0; i < _size; ++i )
        {
            if( _type[i] == AttributeType::eSTRING )
            {
                AttributeType::String* strAttr = ( AttributeType::String* )( array::begin( _default_values ) + _offset[i] );
                string::free_and_null( &strAttr->c_str );
            }
        }


        BX_FREE( bxDefaultAllocator(), _memory_handle );
        _name = nullptr;
        _type = nullptr;
        _offset = nullptr;
        _size = 0;
        _capacity = 0;

        array::clear( _default_values );
        _default_values.~array_t<u8>();
    }

    void AttributeStruct::setDefaultValue( int index, const void* defaulValue )
    {
        const int numBytes = AttributeType::_stride[_type[index]];
        const u8* defaulValueBytes = (u8*)defaulValue;

        for( int ib = 0; ib < numBytes; ++ib )
        {
            array::push_back( _default_values, defaulValueBytes[ib] );
        }
    }

    void AttributeStruct::setDefaultValueString( int index, const char* defaultString )
    {
        char* str = string::duplicate( nullptr, defaultString );
        setDefaultValue( index, &str );
    }

    int AttributeStruct::add( const char* name, AttributeType::Enum typ )
    {
        unsigned nameLen = string::length( name );

        SYS_ASSERT( nameLen <= 8 );
        SYS_ASSERT( typ != AttributeType::eTYPE_COUNT );

        if( _size >= _capacity )
        {
            alloc( _capacity * 2 + 4 );
        }

        int index = _size++;
        _type[index] = typ;
        _name[index].hash = 0;
        memcpy( _name[index].str, name, nameLen );

        const int numBytes = AttributeType::_stride[typ];
        const int offset = array::size( _default_values );
        _offset[index] = offset;
        return index;
    }

    void AttributeInstance::startup( AttributeInstance** outPtr, const AttributeStruct& attrStruct )
    {
        const int n = attrStruct._size;
        if( n == 0 )
        {
            return;
        }

        const int memDataSize = array::size( attrStruct._default_values );
        //for( int i = 0; i < n; ++i )
        //{
        //    memDataSize += attrStruct.stride( i );
        //}

        int memSize = n * sizeof( Value );
        memSize += memDataSize;
        memSize += sizeof( AttributeInstance );

        void* mem = BX_MALLOC( bxDefaultAllocator(), memSize, 4 );
        memset( mem, 0x00, memSize );

        bxBufferChunker chunker( mem, memSize );

        AttributeInstance* attrI = chunker.add< AttributeInstance >();
        attrI->_values = chunker.add< Value >( n );
        attrI->_blob = chunker.addBlock( memDataSize );

        chunker.check();

        attrI->_blob_size_in_bytes = memDataSize;
        attrI->_num_values = n;

        for( int i = 0; i < n; ++i )
        {
            Value& v = attrI->_values[i];
            v.offset = attrStruct._offset[i];
            v.type = attrStruct._type[i];

            const void* defaultValuePtr = array::begin( attrStruct._default_values ) + v.offset;
            if( attrStruct._type[i] != AttributeType::eSTRING )
            {
                attrI->dataSet( i, attrStruct._type[i], defaultValuePtr );
            }
            else
            {
                char** str = (char**)defaultValuePtr;
                attrI->stringSet( i, *str );
            }
        }

        outPtr[0] = attrI;
    }

    void AttributeInstance::shutdown( AttributeInstance** attrIPtr )
    {
        AttributeInstance* attrI = attrIPtr[0];
        if( !attrI )
            return;

        for( int i = 0; i < attrI->_num_values; ++i )
        {
            Value& v = attrI->_values[i];
            if( v.type == AttributeType::eSTRING )
            {
                char** str = (char**)( attrI->_blob + v.offset );
                string::free_and_null( str );
            }
        }

        BX_FREE0( bxDefaultAllocator(), attrIPtr[0] );
    }

    u8* AttributeInstance::pointerGet( int index, AttributeType::Enum type )
    {
        SYS_ASSERT( index < _num_values );
        SYS_ASSERT( _values[index].type == type );

        return _blob + _values[index].offset;
    }

    void AttributeInstance::dataSet( int index, AttributeType::Enum type, const void* data )
    {
        SYS_ASSERT( index < _num_values );
        SYS_ASSERT( _values[index].type == type );
        SYS_ASSERT( type != AttributeType::eSTRING );

        if( type == AttributeType::eINT )
        {
            const int datai = *(const int*)data;
            u8* dst = _blob + _values[index].offset;
            int* dstI = (int*)dst;
            dstI[0] = datai;
        }
        else
        {
            const int stride = AttributeType::_stride[type];
            u8* dst = _blob + _values[index].offset;
            memcpy( dst, data, stride );
        }
    }

    void AttributeInstance::stringSet( int index, const char* str )
    {
        SYS_ASSERT( index < _num_values );
        SYS_ASSERT( _values[index].type == AttributeType::eSTRING );

        char** dstStr = (char**)( _blob + _values[index].offset );
        *dstStr = string::duplicate( *dstStr, str );
    }

    //////////////////////////////////////////////////////////////////////////
}///
