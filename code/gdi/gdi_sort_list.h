#pragma once

#include <util/buffer_utils.h>
#include <util/memory.h>
#include <util/chunk.h>

template< typename Titem >
struct bxGdiSortList
{
    typedef Titem ItemType;
    Titem* items;
	i32 capacity;
	bxAllocator* allocator;
};

namespace bxGdi
{
	template< typename Titem >
	void sortList_new( bxGdiSortList<Titem>** slist, int capacity, bxAllocator* allocator )
	{
		typedef bxGdiSortList<Titem> SortList;
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
    void sortList_delete( bxGdiSortList<Titem>** slist )
	{
        if( !slist[0] )
            return;

        bxAllocator* allocator = slist[0]->allocator;
		BX_FREE0( allocator, slist[0] );
	}

	template< typename Titem >
    int sortList_chunkAdd( bxGdiSortList<Titem>* slist, bxChunk* chunk, Titem item )
	{
		SYS_ASSERT( chunk->end - chunk->current >= 1 );
		const int index = chunk->current++;
		slist->items[index] = item;
		return index;
	}

    template< typename Titem >
    bxChunk sortList_merge( bxGdiSortList<Titem>* slist, bxChunk* chunks, int nChunks )
    {
        //bxChunk* chunks = slistColorChunks;

        bxChunk* frontChunk = chunks;
        bxChunk* backChunk = chunks + (nChunks - 1);
        while ( frontChunk != backChunk )
        {
            while ( frontChunk->current < frontChunk->end )
            {
                if ( backChunk->current <= backChunk->begin )
                {
                    --backChunk;
                }

                if ( frontChunk == backChunk )
                    break;

                if( backChunk->current > backChunk->begin )
                    slist->items[frontChunk->current++] = slist->items[--backChunk->current];
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
    void sortList_sortLess( bxGdiSortList<Titem>* slist, const bxChunk& chunk )
    {
        std::sort( slist->items + chunk.begin, slist->items + chunk.current, std::less<Titem>() );
    }

}
