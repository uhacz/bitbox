#pragma once

#include <util/range_splitter.h>
#include <util/buffer_utils.h>
#include <util/memory.h>

template< typename Titem >
struct bxSortList
{
	Titem* items;
	i32 capacity;
	bxAllocator* allocator;

	struct Chunk
	{
		bxSortList<Titem>* slist;
		i32 begin;
		i32 end;
		i32 current;
	};

};

namespace bx
{
	template< typename Titem >
	void sortListNew( bxSortList<Titem>** slist, int capacity, bxAllocator* allocator )
	{
		typedef bxSortList<Titem> SortList;
		int memSize = 0;
		memSize += sizeof( SortList );
		memSize += capacity * sizeof( Titem );

		void* memHandle = BX_MALLOC( allocator, memSize, ALIGNOF( Titem ) );
		memset( memHandle, 0x00, memSize );
		bxBufferChunker chunker( memHandle, memSize );
		slist[0] = chunker.add< SortList >();
		slist[0]->items = chunker.add< Titem >( capacity );
		chunker.check();

		slist[0]->capacity = capacity;
		slist[0]->allocator = allocator;
	}
	template< typename Titem >
	void sortListDelete( bxSortList<Titem>** slist )
	{
		bxAllocator* allocator = slist[0]->allocator;
		BX_FREE0( allocator, slist[0] );
	}
	template< typename Titem >
	void sortListCreateChunks( typename bxSortList<Titem>::Chunk* chunks, int nChunks, bxSortList<Titem>* slist )
	{
		typedef bxSortList<Titem>::Chunk Chunk;

		const int capacity = slist->capacity;
		bxRangeSplitter split = bxRangeSplitter::splitByTask( capacity, nChunks );

		int ichunk = 0;
		while( split.elementsLeft() )
		{
			const int begin = split.grabbedElements;
			const int grab = split.nextGrab();
			const int end = begin + end;

			SYS_ASSERT( ichunk < nChunks );

			Chunk& c = chunks[ichunk];
			c.slist = slist;
			c.begin = begin;
			c.end = end;
			c.current = begin;

			++ichunk;
		}

		SYS_ASSERT( ichunk == nChunks );
	}
	template< typename Titem >
	int sortListChunkAdd( typename bxSortList<Titem>::Chunk* chunk, Titem item )
	{
		SYS_ASSERT( chunk->end - chunk->current >= 1 );
		const int index = chunk->current++;
		chunk->slist->items[index] = item;
		return index;
	}
}