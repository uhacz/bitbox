#pragma once

#include "memory.h"

class bxMemoryPool
{
public:
    bxMemoryPool();
    ~bxMemoryPool();

    void init( u32 chunkSize, u32 chunkCount, bxAllocator* allocator = bxDefaultAllocator(), u32 alignment = 4 );
    void deinit( bxAllocator* allocator = bxDefaultAllocator() );

    void* alloc();
    void free( void*& ptr );
    size_t allocatedSize()  const { return _allocatedSize; }
    
    u32 maxChunkCount()   const { return _chunkCount; } 
    u32 chunkSize()        const { return _chunkSize; }
    bool empty()            const { return (_chunkCount - _allocatedChunks) == 0; }

    void* begin() { return _pool; }
    void* end() { return (void*)( (u8*)_pool + _chunkSize*_chunkCount ); }

private:
    void* _freeChunk;
	void* _pool;
	
	u32 _allocatedSize;
	u32 _allocatedChunks;
	
	u32 _chunkSize;
	u32 _chunkCount;
};


class bxPoolAllocator : public bxAllocator
{
public:
    bxPoolAllocator();
    virtual ~bxPoolAllocator();
	
    /// Allocator
    virtual void *alloc( size_t size, size_t align );
    virtual void free( void *p );
    virtual size_t allocatedSize()  const;
    
    ///
    void* alloc();

	void startup( u32 chunkSize, u32 chunkCount, bxAllocator* allocator = bxDefaultAllocator(), u32 alignment = 4 );
	void shutdown();
	
private:
    bxMemoryPool _mempool;
    bxAllocator* _baseAllocator;
};

template< class T >
class bxTypedPoolAllocator : public bxPoolAllocator
{
public:
    bxTypedPoolAllocator(): bxPoolAllocator() {}
    virtual ~bxTypedPoolAllocator() {}

    void startup( u32 chunkCount, bxAllocator* allocator = bxDefaultAllocator() )
    {
        bxPoolAllocator::startup( sizeof( T ), chunkCount, allocator );
    }
	void shutdown( bxAllocator* allocator = bxDefaultAllocator() )
    {
        bxPoolAllocator::shutdown( allocator );
    }
};


/////////////////////////////////////////////////////////////////

class bxDynamicPoolAllocator : public bxAllocator
{
public:
    bxDynamicPoolAllocator();
    virtual ~bxDynamicPoolAllocator();

    int startup( u32 chunkSize, u32 chunkCount, bxAllocator* allocator = bxDefaultAllocator(), u32 alignment = 4 );
    void shutdown();

    virtual void *alloc( size_t size, size_t align ) { (void)size; ( void )align; return _alloc(); }
    virtual void free( void *p ) { void* tmp = p; _free( tmp ); }
    virtual size_t allocatedSize() const { return _allocatedSize; }
    
    void* alloc() { return _alloc(); }

private:
    void* _alloc();
    void _free( void*& address );

private:
    struct Node
    {
        Node()
            : next( 0 )
        {}
        bxMemoryPool pool;
        Node* next;
    };

    Node* _begin;
    bxAllocator* _baseAllocator;
    u32 _alignment;

    size_t _allocatedSize;

};


