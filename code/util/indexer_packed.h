#pragma once

#include "array.h"
#include "debug.h"

class bxIndexer_Packed
{
	enum 
	{ 
		eINDEX_MASK = 0xFFFF,
		eNEW_OBJECT_ID_ADD = 0x10000,
	};

    union ID
    {
        u32 hash;
        struct
        {
            u32 index : 16;
            u32 magic : 16;
        };
    };

	struct Index
	{
		u32 id;
		u16 index;
		u16 next;
	};
public:
    bxIndexer_Packed()
		: _numObjects(0)
        , _freelist_enqueue(0)
		, _freelist_dequeue(0)
	{
        u32 fake = add();
        remove( fake );
    }

	inline bool has( u32 id ) const {
        SYS_ASSERT( (id & eINDEX_MASK) < (u32)array::size( _indices ) );
		const Index& in = _indices[id & eINDEX_MASK];
		return in.id == id;
	}
	
    inline u32 index( u32 id ) const {
        SYS_ASSERT( (id & eINDEX_MASK) < (u32)array::size( _indices ) );
        return _indices[id & eINDEX_MASK].index;
    }
    inline u32 index( u32 id ) {
        SYS_ASSERT( (id & eINDEX_MASK) < (u32)array::size( _indices ) );
        return _indices[id & eINDEX_MASK].index;
    }

	u32 add()
	{
        const u16 objectIndex = _numObjects++;
        if ( _freelist_dequeue >= array::size( _indices ) )
		{
			const u16 last_index = (u16)array::size( _indices );
			_freelist_dequeue = array::push_back( _indices, Index() );

			Index& in =array::back(  _indices );
			in.id = last_index + eNEW_OBJECT_ID_ADD;
			in.next = UINT16_MAX;
            in.index = objectIndex;

			_freelist_enqueue = last_index;

			if( last_index > 0 )
			{
				_indices[last_index-1].next = last_index;
			}
		}

		Index& in = _indices[_freelist_dequeue];
        ID id = { 0 };
        in.index = objectIndex;
		
		_freelist_dequeue = in.next;

		return in.id;
	}

	void remove( u32& id )
	{
        if ( !_numObjects )
            return;
        
        SYS_ASSERT( !array::empty( _indices ) );
        SYS_ASSERT( (id & eINDEX_MASK) < (u32)array::size( _indices ) );

		Index& in = _indices[ id & eINDEX_MASK ];
        Index& lastIdx = _indices[--_numObjects];
		
        lastIdx.index = in.index;

		in.id += eNEW_OBJECT_ID_ADD;
        in.index = UINT16_MAX;

		if( _freelist_dequeue >= array::size( _indices ) )
		{
			_freelist_enqueue = id & eINDEX_MASK;
			_freelist_dequeue = _freelist_enqueue;	
			_indices[_freelist_dequeue].next = UINT16_MAX;
		}
		else
		{
			_indices[_freelist_enqueue].next = id & eINDEX_MASK;
			_freelist_enqueue = id & eINDEX_MASK;
		}
		id = UINT32_MAX;
	}

    u32 size() const  { return array::size( _indices ); }

private:
	array_t<Index> _indices;
    u16 _numObjects;
	u16 _freelist_enqueue;
	u16 _freelist_dequeue;
    u16 __padding;
};
