#include "gdi_backend.h"
#include <string.h>
#include <memory.h>

bxGdiVertexStreamDesc::bxGdiVertexStreamDesc()
{
    count = 0;
    memset( blocks, 0, sizeof( blocks ) );
}
void bxGdiVertexStreamDesc::addBlock( bxGdi::EVertexSlot slot, bxGdi::EDataType type, int numElements, int norm )
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
        blocks[index].typeNorm = norm;
        blocks[index].numElements = numElements;
        ++count;
    }
}

namespace bxGdi
{
    EDataType typeFromName( const char* name )
    {
        for( int itype = 0; itype < eTYPE_COUNT; ++itype )
        {   
            if ( !strcmp( name, typeName[itype] ) )
                return (EDataType)itype;
        }
        return eTYPE_UNKNOWN;
    }
    EDataType findBaseType( const char* name )
    {
        for ( int itype = 0; itype < eTYPE_COUNT; ++itype )
        {
            if ( strstr( name, typeName[itype] ) )
                return (EDataType)itype;
        }
        return eTYPE_UNKNOWN;
    }

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

namespace bxGdi
{
    unsigned char* buffer_map( bxGdiContextBackend* ctx, bxGdiBuffer buffer, int firstElement, int numElements, int mapType )
    {
        SYS_ASSERT( (buffer.bindFlags & eBIND_CONSTANT_BUFFER) == 0 );
        SYS_ASSERT( ( firstElement + numElements ) * formatByteWidth(buffer.format) <= (int)buffer.sizeInBytes );

        const int offsetInBytes = firstElement * formatByteWidth( buffer.format );
        return ctx->map( buffer.rs, offsetInBytes, mapType );
    }
}///

#include "dx11/gdi_backend_dx11_startup.h"
#include <util/memory.h>
namespace bxGdi
{
    int backendStartup( bxGdiDeviceBackend** dev, uptr hWnd, int winWidth, int winHeight, int fullScreen )
    {
        return startup_dx11( dev, hWnd, winWidth, winHeight, fullScreen );
    }
    void backendShutdown( bxGdiDeviceBackend** dev )
    {
        BX_DELETE0( bxDefaultAllocator(), dev[0]->ctx );
        BX_DELETE0( bxDefaultAllocator(), dev[0] );
    }

    EVertexSlot vertexSlotFromString( const char* n )
    {
        for( int i = 0; i < eSLOT_COUNT; ++i )
        {
            if( !strcmp( n, slotName[i] ) )
                return (EVertexSlot)i;
        }

        return eSLOT_COUNT;
    }

}///
