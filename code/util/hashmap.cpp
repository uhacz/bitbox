#include "hashmap.h"
#include "debug.h"
#include "array.h"

hashmap_t::hashmap_t( int initSize, bxAllocator* alloc )
    : allocator( alloc )
{
    cells = (cell_t*)BX_MALLOC( alloc, initSize * sizeof( cell_t ), ALIGNOF( cell_t ) );
    capacity = initSize;
    size = 0;
    memset( cells, 0, capacity* sizeof( cell_t ) );
}
hashmap_t::~hashmap_t()
{
    BX_FREE0( allocator, cells );
}


namespace hashmap
{
    
    #define FIRST_CELL(hash) ( hmap.cells + ((hash) & (hmap.capacity - 1)))
    #define CIRCULAR_NEXT(c) ((c) + 1 != ( hmap.cells ) + hmap.capacity ? (c) + 1 : ( hmap.cells ))
    #define CIRCULAR_OFFSET(a, b) ((b) >= (a) ? (b) - (a) : hmap.capacity + (b) - (a))

    typedef hashmap_t::cell_t Cell;
    inline uint64_t integerHash(uint64_t k)
    {
	    k ^= k >> 33;
	    k *= 0xff51afd7ed558ccd;
	    k ^= k >> 33;
	    k *= 0xc4ceb9fe1a85ec53;
	    k ^= k >> 33;
	    return k;
    }

    void _Grow( hashmap_t& hmap, size_t desiredSize)
    {
        SYS_ASSERT((desiredSize & (desiredSize - 1)) == 0);   // Must be a power of 2
        SYS_ASSERT( hmap.size * 4  <= desiredSize * 3);

        // Get start/end pointers of old array
        Cell* oldCells = hmap.cells;
        Cell* end = hmap.cells + hmap.capacity;

        // Allocate new array
        hmap.capacity = desiredSize;
        hmap.cells = (Cell*)BX_MALLOC( hmap.allocator, hmap.capacity * sizeof( Cell ), ALIGNOF( Cell ) );
        memset( hmap.cells, 0, sizeof(Cell) * hmap.capacity );

        // Iterate through old array
        for (Cell* c = oldCells; c != end; c++)
        {
            if (c->key)
            {
                // Insert this element into new array
                for (Cell* cell = FIRST_CELL(integerHash(c->key));; cell = CIRCULAR_NEXT(cell))
                {
                    if (!cell->key)
                    {
                        // Insert here
                        *cell = *c;
                        break;
                    }
                }
            }
        }

        // Delete old array
        BX_FREE0( hmap.allocator, oldCells );
    }

    /////////////////////////////////////////////////////////////
    hashmap_t::cell_t* lookup( hashmap_t& hmap, size_t key)
    {
        if( !key )
            return 0;

        // Check regular cells
        for ( Cell* cell = FIRST_CELL(integerHash(key));; cell = CIRCULAR_NEXT(cell))
        {
            if (cell->key == key)
                return cell;
            if (!cell->key)
                return 0;
        }

        return 0;
    }
    const hashmap_t::cell_t* lookup( const hashmap_t& hmap, size_t key )
    {
        if ( !key )
            return 0;

        // Check regular cells
        for ( Cell* cell = FIRST_CELL( integerHash( key ) );; cell = CIRCULAR_NEXT( cell ) )
        {
            if ( cell->key == key )
                return cell;
            if ( !cell->key )
                return 0;
        }

        return 0;
    }
    hashmap_t::cell_t* insert( hashmap_t& hmap, size_t key)
    {
        if( !key )
            return 0;

        // Check regular cells
        for (;;)
        {
            for (Cell* cell = FIRST_CELL(integerHash(key));; cell = CIRCULAR_NEXT(cell))
            {
                if (cell->key == key)
                    return cell;        // Found
                if (cell->key == 0)
                {
                    // Insert here
                    if ((hmap.size + 1) * 4 >= hmap.capacity * 3)
                    {
                        // Time to resize
                        _Grow( hmap, hmap.capacity * 2 );
                        break;
                    }
                    ++hmap.size;
                    cell->key = key;
                    return cell;
                }
            }
        }
    
        return 0;
    }
    void erase( hashmap_t& hmap, hashmap_t::cell_t* cell)
    {
        if( !cell )
            return;

        // Delete from regular cells
        SYS_ASSERT(cell >= hmap.cells && (size_t)( cell - hmap.cells) < hmap.capacity );
        SYS_ASSERT(cell->key != 0);

        // Remove this cell by shuffling neighboring cells so there are no gaps in anyone's probe chain
        for (Cell* neighbor = CIRCULAR_NEXT(cell);; neighbor = CIRCULAR_NEXT(neighbor))
        {
            if (!neighbor->key)
            {
                // There's nobody to swap with. Go ahead and clear this cell, then return
                cell->key = 0;
                cell->value = 0;
                hmap.size--;
                return;
            }
            Cell* ideal = FIRST_CELL(integerHash(neighbor->key));
            if (CIRCULAR_OFFSET(ideal, cell) < CIRCULAR_OFFSET(ideal, neighbor))
            {
                // Swap with neighbor, then make neighbor the new cell to remove.
                *cell = *neighbor;
                cell = neighbor;
            }
        }
    }
    void clear( hashmap_t& hmap )
    {
        memset( hmap.cells, 0, sizeof(Cell) * hmap.capacity );
        hmap.size = 0;
    }

    //----------------------------------------------
    //  Iterator
    //----------------------------------------------
    iterator::iterator(hashmap_t &hm )
        :_hmap( hm )
    {
        _cur = &hm.cells[-1];
        //next();
    }

    hashmap_t::cell_t* iterator::next()
    {
        // Already finished?
        if (!_cur)
            return _cur;

        // Iterate through the regular cells
        Cell* end = _hmap.cells + _hmap.capacity;
        while (++_cur != end)
        {
            if (_cur->key)
                return _cur;
        }

        // Finished
        return _cur = NULL;
    }

}