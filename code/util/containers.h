#pragma once

#include "type.h"
#include "memory.h"

template< typename T >
struct array_t
{
    u32 size;
    u32 capacity;
    T* data;
    allocator_t* allocator;

    array_t( allocator_t* alloc = bxDefaultAllocator() );

    T &operator[]( int i);
	const T &operator[]( int i) const;
};

template< typename T >
struct queue_t
{
    array_t<T> data;
    u32 size;
    u32 offset;

    queue_t( allocator_t* alloc = bxDefaultAllocator() );

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