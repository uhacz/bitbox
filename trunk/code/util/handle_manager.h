#pragma once

#include "type.h"
#include "common.h"
#include "memory.h"

template<class TpType, u32 TpGranularity = 32, u32 TpNIndexBits = 14>
class bxHandleManager
{
private:
    enum 
    {
        eHANDLE_BITS = 30,
    };
    struct Entry
    {
        Entry()
            : _nextFreeIndex(0)
            , _counter(1)
            , _active(0)
            , _endOfList(0)
            , _entry(0)
        {
        }

        explicit Entry( u32 nextFreeIndex)
            : _nextFreeIndex(nextFreeIndex)
            , _counter(1)
            , _active(0)
            , _endOfList(0)
            , _entry(0)
        {}

        u32 _nextFreeIndex : TpNIndexBits;
        u32 _counter       : (eHANDLE_BITS-(TpNIndexBits));
        u32 _active        : 1;
        u32 _endOfList     : 1;
        TpType _entry;
    };

public:
    union Handle
    {
    public:
        u32 _hash;
        struct
        {
            u32 _index : TpNIndexBits;
            u32 _counter : (eHANDLE_BITS - ( TpNIndexBits ) );
        };


    public:
        Handle()
            : _index(0), _counter(0)
        {
        }

        Handle(u32 index, u32 counter )
            : _index(index), _counter(counter)
        {}

        explicit Handle( u32 h )
            : _hash( h )
        {}

        u32 index() const
        {
            return _index;
        }

        u32 asU32() const
        {
            return _hash;
        }

        bool operator == ( Handle rhs ) const { return _hash == rhs._hash; }
        bool operator != ( Handle rhs ) const { return _hash != rhs._hash; }
    };

public:
    bxHandleManager()
        : _entries( NULL )
        , _capacity( 0 )
        , _activeEntryCount( 0 )
        , _firstFreeEntry( 0 )
    {
        SYS_STATIC_ASSERT( sizeof(Handle) == 4 );
    }
    ~bxHandleManager()
    {
        reset();
    }
    ////
    ////
    void reset()
    {
        SYS_ASSERT( _activeEntryCount == 0 && "not all handles released!" );

        BX_FREE0( bxDefaultAllocator(), _entries );
        _capacity = 0;

        _activeEntryCount = 0;
        _firstFreeEntry = 0;
    }
    ////
    ////
    Handle add( const TpType& p )
    {
        if ( _activeEntryCount+1 >= _capacity )
            _AllocateMore( bxDefaultAllocator() );

        SYS_ASSERT(_activeEntryCount < eABSOLUTE_MAX_ENTRIES - 1);

        const uint32_t newIndex = _firstFreeEntry;
        SYS_ASSERT(newIndex < eABSOLUTE_MAX_ENTRIES);
        SYS_ASSERT(_entries[newIndex]._active == 0);
        SYS_ASSERT(!_entries[newIndex]._endOfList);

        _firstFreeEntry = _entries[newIndex]._nextFreeIndex;
        _entries[newIndex]._nextFreeIndex = 0;
        //_entries[newIndex]._counter = _entries[newIndex]._counter + 1;
        
        if (_entries[newIndex]._counter == 0)
        {
            _entries[newIndex]._counter = 1;
        }
        
        _entries[newIndex]._active = 1;
        _entries[newIndex]._entry = p;

        ++ _activeEntryCount;

        return Handle( newIndex, _entries[newIndex]._counter );
    }
    ////
    ////
    void update( Handle handle, const TpType p )
    {
        const u32 index = handle._index;
        SYS_ASSERT(_entries[index]._counter == handle._counter);
        SYS_ASSERT(_entries[index]._active == 1);
        _entries[index]._entry = p;
    }
    ////
    ////
    void remove( Handle& handle )
    {
        const u32 index = handle._index;
        SYS_ASSERT( _entries[index]._counter == handle._counter );
        SYS_ASSERT( _entries[index]._active == 1);

        _entries[index]._nextFreeIndex = _firstFreeEntry;
        _entries[index]._active = 0;
        _entries[index]._counter += 1;
        _firstFreeEntry = index;

        --_activeEntryCount;

        handle = Handle();
    }
    ////
    ////
    void removeByIndex( const u32 index )
    {
        SYS_ASSERT( index < eABSOLUTE_MAX_ENTRIES-1 && _entries[index]._active == 1 );

        _entries[index]._nextFreeIndex = _firstFreeEntry;
        _entries[index]._active = 0;
        _entries[index]._counter += 1;
        _firstFreeEntry = index;

        --_activeEntryCount;
    }
    ////
    ////
    const TpType& get( const Handle& handle ) const
    {
        return _entries[handle.index]._entry;
    }
    
    ////
    ////
    int get( Handle handle, TpType* out ) const
    {
        const uint32_t index = handle._index;

        if ( index >= _capacity )
            return 0;

        const Entry& e = _entries[index];
        if ( e._counter != handle._counter || e._active == 0 )
            return 0;

        out = e._entry;
        return 1;
    }
    ////
    ////
    Handle getByIndex( u32 index ) const
    {
        SYS_ASSERT( index < eABSOLUTE_MAX_ENTRIES-1 );
        const Entry& e = _entries[index];
        Handle h( index, e._counter );
        return h;
    }
    ////
    ////
    uint32_t getCount() const
    {
        return _activeEntryCount;
    }
    ////
    ////
    int isValid( Handle handle )
    {
        const uint32_t index = handle._index;

        if ( index >= _capacity )
            return 0;

        const Entry& e = _entries[index];
        return ( e._counter != handle._counter || e._active == 0 ) ? 0 : 1;
    }


private:
    enum 
    { 
        eABSOLUTE_MAX_ENTRIES = 1 << TpNIndexBits 
    };
    
    bxHandleManager(const bxHandleManager&);
    bxHandleManager& operator=(const bxHandleManager&);
    ////
    ////
    void _AllocateMore( bxAllocator* allocator )
    {
        uint32_t newCount = minOfPair( _capacity + TpGranularity, (u32)(eABSOLUTE_MAX_ENTRIES-1) );
        Entry* newEntries = (Entry*)BX_MALLOC( allocator, sizeof(Entry)*newCount, 64 ); //reinterpret_cast<Entry*>( picoMallocAligned(, 64) );
        if ( _capacity )
        {
            memcpy( newEntries, _entries, sizeof(Entry) * (_capacity-1) );
        }
        BX_FREE0( allocator, _entries );
        _entries = newEntries;

        uint32_t startIndex = _capacity ? _capacity-1 : 0;

        for ( uint32_t ie = startIndex; ie < newCount-1; ++ie )
        {
            _entries[ie] = Entry(ie+1);
        }

        _entries[newCount - 1] = Entry();
        _entries[newCount - 1]._endOfList = 1;

        _capacity = newCount;
    }

private:
    Entry* _entries;
    u32 _capacity;

    u32 _activeEntryCount;
    u32 _firstFreeEntry;
};
