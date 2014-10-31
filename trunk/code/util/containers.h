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
    
    array_t( bxAllocator* alloc = bxDefaultAllocator() );
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

    queue_t( bxAllocator* alloc = bxDefaultAllocator() );

    T &operator[]( int i);
	const T &operator[]( int i) const;
};

struct hashmap_t
{
    struct Cell
    {
        u64 key;
        u64 value;
    };

    array_t<Cell> cells;
};