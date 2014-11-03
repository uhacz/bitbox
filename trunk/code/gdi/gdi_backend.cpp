#include "gdi_backend.h"
#include <memory.h>

bxGdiVertexStreamDesc::bxGdiVertexStreamDesc()
{
    count = 0;
    memset( blocks, 0, sizeof( blocks ) );
}
void bxGdiVertexStreamDesc::addBlock( bxGdi::EVertexSlot slot, bxGdi::EDataType type, int numElements )
{
    if( count >= eMAX_BLOCKS )
    {
        bxLogError( "can not add block to vertex stream " );
    }
    else
    {
        const int index = count;
        blocks[index].slot = slot;
        blocks[index].dataType = type;
        blocks[index].numElements = numElements;
        ++count;
    }
}

namespace bxGdi
{
    int blockStride( const bxGdiVertexStreamBlock& block )
    { 
        return typeStride[block.dataType] * block.numElements; 
    }
    int streamStride(const bxGdiVertexStreamDesc& vdesc )
    {
        int stride = 0;
        for( int i = 0; i < vdesc.count; ++i )
        {
            stride += blockStride( vdesc.blocks[i] );
        }
        return stride;
    }
    unsigned streamSlotMask( const bxGdiVertexStreamDesc& vdesc )
    {
        unsigned mask = 0;
        for( int i = 0; i < vdesc.count; ++i )
        {
            SYS_ASSERT( vdesc.blocks[i].slot < eSLOT_COUNT );
            mask |= 1 << vdesc.blocks[i].slot;
        }
        return mask;
    }

}///