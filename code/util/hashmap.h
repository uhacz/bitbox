#pragma once
 
#include "containers.h"

namespace hashmap
{
          hashmap_t::cell_t* lookup(       hashmap_t& hmap, size_t key);
    const hashmap_t::cell_t* lookup( const hashmap_t& hmap, size_t key );
    
    hashmap_t::cell_t* insert( hashmap_t& hmap, size_t key);
    hashmap_t::cell_t* set   ( hashmap_t& hmap, size_t key, size_t value );

    void erase( hashmap_t& hmap, hashmap_t::cell_t* cell);
    void clear( hashmap_t& hmap );
    void reserve( hashmap_t& hmap, size_t desiredSize );

    inline bool empty( const hashmap_t& hmap ) { return hmap.size == 0; }
    inline int size( const hashmap_t& hmap ) { return (int)hmap.size; }
    inline void eraseByKey( hashmap_t& hmap, size_t key)
    {
        hashmap_t::cell_t* value = lookup( hmap, key);
        if (value)
        {
            erase( hmap, value);
        }
    }

    //----------------------------------------------
    //  Iterator
    //----------------------------------------------
    class iterator
    {
    private:
        hashmap_t& _hmap;
        hashmap_t::cell_t* _cur;

    public:
        iterator(hashmap_t &hm );

        hashmap_t::cell_t* next();
        inline hashmap_t::cell_t* operator*() const { return _cur; }
        inline hashmap_t::cell_t* operator->() const { return _cur; }
    };
}///
