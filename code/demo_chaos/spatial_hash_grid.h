#pragma once

#include <util/type.h>

namespace bx
{

struct HashGridStatic
{
    struct Entry
    {
        u32 bucket_index;
        u32 data;
    };

    Entry* _entries = nullptr;
    u32  _= 0;



};
namespace hash_grid
{
    HashGridStatic* Create( unsigned hashTableSize );
    void Destroy( HashGridStatic** hg );

    void Clear( HashGridStatic* hg );
    void Insert( HashGridStatic* hg );
}//

}//