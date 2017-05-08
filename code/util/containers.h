#pragma once

#include "type.h"
#include "memory.h"

template< typename T >
struct array_t
{
    u32 size;
    u32 capacity;
    bxAllocator* allocator;
    T* data;
    
    explicit array_t<T>::array_t( bxAllocator* alloc = bxDefaultAllocator() )
        : size( 0 ), capacity( 0 ), allocator( alloc ), data( 0 ) 
    {}

    ~array_t()
    {
        BX_FREE0( allocator, data );
    }

          T &operator[]( int i) { return data[i]; }
    const T &operator[]( int i) const { return data[i]; }

    T* begin() { return data; }
    T* end  () { return data + size; }

    const T* begin() const { return data; }
    const T* end  () const { return data + size; }
};

template< typename T >
struct queue_t
{
    array_t<T> data;
    u32 size;
    u32 offset;

    explicit queue_t( bxAllocator* alloc = bxDefaultAllocator() ) 
        : data( alloc ) , size( 0 ) , offset( 0 )
    {}
    ~queue_t()
    {}

          T& operator[]( int i )        { return data[(i + offset) % data.size]; }
    const T& operator[]( int i ) const  { return data[(i + offset) % data.size]; }
};

struct hashmap_t
{
    struct cell_t
    {
        u64 key;
        u64 value;
    };

    cell_t* cells;
    bxAllocator* allocator;
    
    size_t size;
    size_t capacity;

    hashmap_t( int initSize = 16, bxAllocator* alloc = bxDefaultAllocator() ) 
        : allocator( alloc )
    {
        cells = (cell_t*)BX_MALLOC( alloc, initSize * sizeof( cell_t ), ALIGNOF( cell_t ) );
        capacity = initSize;
        size = 0;
        for( u32 i = 0; i < capacity; ++i )
        {
            cells[i].key = 0;
            cells[i].value = 0;
        }
    }
    hashmap_t::~hashmap_t()
    {
        BX_FREE0( allocator, cells );
    }
};


#define BX_INVALID_ID UINT16_MAX
union id_t
{
    u32 hash;
    struct{
        u16 id;
        u16 index;
    };
};
inline bool operator == ( id_t a, id_t b ) { return a.hash == b.hash; }

inline id_t make_id( u32 hash ){
    id_t id = { hash };
    return id;
}

template <u32 MAX, typename Tid = id_t >
struct id_array_t
{
    id_array_t() : _freelist( BX_INVALID_ID ) , _next_id( 0 ) , _size( 0 )
    {
        for( u32 i = 0; i < MAX; i++ )
        {
            _sparse[i].id = BX_INVALID_ID;
        }
    }

    //T& operator[]( u32 index );
    //const T& operator[]( u32 index ) const;

    u16 _freelist;
    u16 _next_id;
    u16 _size;

    Tid _sparse[MAX];
    u16 _sparse_to_dense[MAX];
    u16 _dense_to_sparse[MAX];
    //T _objects[MAX];
};

template <u32 MAX, typename Tid = id_t>
struct id_table_t
{
    id_table_t() : _freelist( BX_INVALID_ID ) , _next_id( 0 ) , _size( 0 )
    {
        for( u32 i = 0; i < MAX; i++ )
        {
            _ids[i].id = BX_INVALID_ID;
        }
    }

    u16 _freelist;

    u16 _next_id;
    u16 _size;

    Tid _ids[MAX];
};

template< typename T > T makeInvalidHandle()
{
    T h = { 0 };
    return h;
}
