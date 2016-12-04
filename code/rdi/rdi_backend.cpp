#include "rdi_backend.h"

#include "rdi_backend_dx11.h"


namespace bx{ namespace rdi{ 


EDataType::Enum EDataType::FromName( const char* name )
{
    for( int itype = 0; itype < EDataType::COUNT; ++itype )
    {
        if( !strcmp( name, EDataType::name[itype] ) )
            return (EDataType::Enum)itype;
    }
    return EDataType::UNKNOWN;
}

EDataType::Enum EDataType::FindBaseType( const char* name )
{
    for( int itype = 0; itype < EDataType::COUNT; ++itype )
    {
        if( strstr( name, EDataType::name[itype] ) )
            return (EDataType::Enum)itype;
    }
    return EDataType::UNKNOWN;
}


EVertexSlot::Enum EVertexSlot::FromString( const char* n )
{
    for( int i = 0; i < EVertexSlot::COUNT; ++i )
    {
        if( !strcmp( n, EVertexSlot::name[i] ) )
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