#include "rdi_backend.h"

#include "rdi_backend_dx11.h"


namespace bx{ namespace rdi{ 

EDataType::Enum typeFromName( const char* name )
{
    for( int itype = 0; itype < EDataType::COUNT; ++itype )
    {
        if( !strcmp( name, typeName[itype] ) )
            return (EDataType::Enum)itype;
    }
    return EDataType::UNKNOWN;
}

EDataType::Enum findBaseType( const char* name )
{
    for( int itype = 0; itype < EDataType::COUNT; ++itype )
    {
        if( strstr( name, typeName[itype] ) )
            return (EDataType::Enum)itype;
    }
    return EDataType::UNKNOWN;
}


EVertexSlot::Enum vertexSlotFromString( const char* n )
{
    for( int i = 0; i < EVertexSlot::COUNT; ++i )
    {
        if( !strcmp( n, slotName[i] ) )
            return ( EVertexSlot::Enum )i;
    }

    return EVertexSlot::COUNT;
}

void Startup( uptr hWnd, int winWidth, int winHeight, int fullScreen )
{
    StartupDX11( hWnd, winWidth, winHeight, fullScreen );
}

void Shutdown()
{
    ShutdownDX11();
}

}
}///