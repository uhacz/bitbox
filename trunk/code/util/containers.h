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