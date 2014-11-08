#include "containers.h"

namespace hashmap
{
    hashmap_t::cell_t* lookup( hashmap_t& hmap, size_t key);
    hashmap_t::cell_t* insert( hashmap_t& hmap, size_t key);

    void erase( hashmap_t& hmap, hashmap_t::cell_t* cell);
    void clear( hashmap_t& hmap );

    bool empty( hashmap_t& hmap ) { return hmap.size == 0; }
    
    inline void erase( hashmap_t& hmap, size_t key)
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
