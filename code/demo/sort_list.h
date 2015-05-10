#pragma once

#include <util/range_splitter.h>
#include <util/buffer_utils.h>
#include <util/memory.h>

template< typename Titem >
struct bxSortList
{
    typedef Titem ItemType;
    Titem* items;
	i32 capacity;
	bxAllocator* allocator;
};

struct bxChunk
{
    i32 begin;
    i32 end;
    i32 current;
};

namespace bx
{
    inline void chunk_create( bxChunk* chunks, int nChunks, int nItems )
    {
        const int N_TASKS = nChunks;
        bxRangeSplitter splitter = bxRangeSplitter::splitByTask( nItems, N_TASKS );
        int ilist = 0;
        while ( splitter.elementsLeft() )
        {
            const int begin = splitter.grabbedElements;
            const int grab = splitter.nextGrab();
            const int end = begin + grab;

            SYS_ASSERT( ilist < N_TASKS );
            bxChunk& c = chunks[ilist++];
            c.begin = begin;
            c.end = end;
            c.current = begin;
        }

        SYS_ASSERT( ilist <= N_TASKS );
    }


	template< typename Titem >
	void sortList_new( bxSortList<Titem>** slist, int capacity, bxAllocator* allocator )
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
	void sortList_delete( bxSortList<Titem>** slist )
	{
		bxAllocator* allocator = slist[0]->allocator;
		BX_FREE0( allocator, slist[0] );
	}

	template< typename Titem >
	int sortList_chunkAdd( bxSortList<Titem>* slist, bxChunk* chunk, Titem item )
	{
		SYS_ASSERT( chunk->end - chunk->current >= 1 );
		const int index = chunk->current++;
		slist->items[index] = item;
		return index;
	}

    template< typename Titem >
    bxChunk sortList_merge( bxSortList<Titem>* slist, bxChunk* chunks, int nChunks )
    {
        //bxChunk* chunks = slistColorChunks;

        bxChunk* frontChunk = chunks;
        bxChunk* backChunk = chunks + (nChunks - 1);
        while ( frontChunk != backChunk )
        {
            while ( frontChunk->current < frontChunk->end )
            {
                slist->items[frontChunk->current++] = slist->items[--backChunk->current];
                if ( backChunk->current <= backChunk->begin )
                {
                    --backChunk;
                }

                if ( frontChunk == backChunk )
                    break;
            }

            if ( frontChunk == backChunk )
                break;
            else
                ++frontChunk;
        }

        int nSortItems = chunks[0].current;
        for ( int ichunk = 1; ichunk < nChunks; ++ichunk )
        {
            const bxChunk& c = chunks[ichunk];
            if ( c.current == c.begin )
                break;

            nSortItems = c.current;
        }
        bxChunk finalChunk;
        finalChunk.begin = 0;
        finalChunk.current = nSortItems;
        finalChunk.end = nSortItems;
        
        return finalChunk;
    }

    template< typename Titem >
    void sortList_sortLess( bxSortList<Titem>* slist, const bxChunk& chunk )
    {
        std::sort( slist->items + chunk.begin, slist->items + chunk.current, std::less<Titem>() );
    }

}
