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
    
    explicit array_t( bxAllocator* alloc = bxDefaultAllocator() );
    ~array_t();

    T &operator[]( int i) { return data[i]; }
    const T &operator[]( int i) const { return data[i]; }
};

template< typename T >
struct queue_t
{
    array_t<T> data;
    u32 size;
    u32 offset;

    explicit queue_t( bxAllocator* alloc = bxDefaultAllocator() );
    ~queue_t();

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

    hashmap_t( int initSize = 16, bxAllocator* alloc = bxDefaultAllocator() );
    ~hashmap_t();
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

template <typename T, u32 MAX >
struct id_array_t
{
    id_array_t();

    T& operator[]( u32 index );
    const T& operator[]( u32 index ) const;

    u16 _freelist;
    u16 _next_id;
    u16 _size;

    id_t _sparse[MAX];
    u16 _sparse_to_dense[MAX];
    u16 _dense_to_sparse[MAX];
    T _objects[MAX];
};

template <u32 MAX>
struct id_table_t
{
    id_table_t();

    u16 _freelist;

    u16 _next_id;
    u16 _size;

    id_t _ids[MAX];
};
