#include "pool_allocator.h"
#include "debug.h"
#include "memory.h"
#include <new>

bxMemoryPool::bxMemoryPool()
    : _freeChunk(0), _pool(0)
    , _chunkSize(0), _chunkCount(0)
	, _allocatedSize(0), _allocatedChunks(0)
{}
bxMemoryPool::~bxMemoryPool()
{}

void bxMemoryPool::init( u32 chunkSize, u32 chunkCount, bxAllocator* allocator, u32 alignment )
{
    SYS_ASSERT( chunkSize >= 4 );
	SYS_ASSERT( chunkCount > 0 );

	const u32 poolSize8 = chunkSize * chunkCount;

	_chunkSize = chunkSize;
	_chunkCount = chunkCount;

	_pool = BX_MALLOC( allocator, poolSize8, alignment );
	SYS_ASSERT( _pool != NULL );

	_freeChunk = _pool;

	uptr offset8 = (uptr)_pool + poolSize8 - chunkSize;
	uptr rawLastPointer = 0;

	while ( (void*)offset8 != _pool )
	{
		*((uptr*)offset8) = rawLastPointer;
		rawLastPointer = offset8;
		offset8 -= chunkSize;
	}

	*((uptr*)_pool) = rawLastPointer;

}
void bxMemoryPool::deinit( bxAllocator* allocator )
{
    SYS_ASSERT( _allocatedSize == 0 );
    BX_FREE( allocator, _pool );

    _freeChunk = 0;
    _pool = 0;

    _allocatedSize = 0;
    _allocatedChunks = 0;

    _chunkSize = 0;
    _chunkCount = 0;
}

void* bxMemoryPool::alloc()
{
    if (_freeChunk == 0) 
	{ 
		SYS_ASSERT(!"Out of memory"); 
		return 0; 
	}

	void* result = _freeChunk;
	_freeChunk = (void*)*((uptr*)_freeChunk);
	++_allocatedChunks;
	_allocatedSize += _chunkSize;
	return result;
}
void bxMemoryPool::free( void*& address )
{
    SYS_ASSERT( (iptr)address >= (iptr)_pool );
	SYS_ASSERT( (iptr)address < (iptr)((u8*)_pool + _chunkCount*_chunkSize ) );
	
	*((uptr*)address) = (uptr)_freeChunk;
	_freeChunk = address;
	
	--_allocatedChunks;
	_allocatedSize -= _chunkSize;

    address = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bxPoolAllocator::bxPoolAllocator()
    : _baseAllocator(0)
{}

bxPoolAllocator::~bxPoolAllocator()
{}

void * bxPoolAllocator::alloc( size_t size, size_t align )
{
    (void)size; (void)align; return _mempool.alloc();
}

void* bxPoolAllocator::alloc()
{
    return _mempool.alloc();
}

void bxPoolAllocator::free( void *p )
{
    void* tmp = p; _mempool.free( tmp );
}

size_t bxPoolAllocator::allocatedSize() const
{
    return _mempool.allocatedSize();
}

void bxPoolAllocator::startup( u32 chunkSize, u32 chunkCount, bxAllocator* allocator /*= bxDefaultAllocator()*/, u32 alignment /*= 4 */ )
{
    _baseAllocator = allocator;
    _mempool.init( chunkSize, chunkCount, allocator, alignment );
}

void bxPoolAllocator::shutdown()
{
    _mempool.deinit( _baseAllocator );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bxDynamicPoolAllocator::bxDynamicPoolAllocator()
: _begin(0)
, _baseAllocator(0)
, _alignment(0)
, _allocatedSize(0)
{}

bxDynamicPoolAllocator::~bxDynamicPoolAllocator()
{
    SYS_ASSERT( _begin == 0 );
}

/////////////////////////////////////////////////////////////////
int bxDynamicPoolAllocator::startup( u32 chunkSize, u32 chunkCount, bxAllocator* allocator, u32 alignment )
{
    _begin = BX_NEW( allocator, Node );
    _begin->pool.init( chunkSize, chunkCount, allocator, alignment );

    _baseAllocator = allocator;
    _alignment = alignment;

    return 0;
}
void bxDynamicPoolAllocator::shutdown()
{
    Node* node = _begin;
    while( node )
    {
        node->pool.deinit( _baseAllocator );
        Node* tmp = node;
        node = node->next;

        BX_DELETE( _baseAllocator, tmp );
    }

    _begin = 0;
    _baseAllocator = 0;
    _alignment = 0;

}

void* bxDynamicPoolAllocator::_alloc()
{
    Node* prev_node = _begin;
    Node* node = _begin;
    while( node->pool.empty() )
    {
        prev_node = node;
        node = node->next;
        if( !node )
            break;
    }
        
    if( !node )
    {
        const bxMemoryPool& beginPool = _begin->pool;
        node = BX_NEW( _baseAllocator, Node );
        node->pool.init( beginPool.chunkSize(), beginPool.maxChunkCount(), _baseAllocator, _alignment );

        prev_node->next = node;
    }
    
    _allocatedSize += _begin->pool.chunkSize();

    return node->pool.alloc();
}

void bxDynamicPoolAllocator::_free( void*& address )
{
    Node* node = _begin;

    while( node )
    {
        void* b = node->pool.begin();
        void* e = node->pool.end();

        if( b <= address && e > address )
        {
            break;
        }

        node = node->next;
    }

    if( !node )
    {
        bxLogError( "Bad address" );
    }
    else
    {
        _allocatedSize -= node->pool.chunkSize();
        SYS_ASSERT( _allocatedSize >= 0 );
        node->pool.free( address );
    }
}

