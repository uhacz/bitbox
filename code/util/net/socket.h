#pragma once

namespace bx
{
using socket_t  = size_t;
using address_t = size_t;

struct IP4Str
{
    char c_str[22];
};

namespace ESocket
{
    enum Type
    {
        TCP,
        UDP,
    };
}//

address_t CreateAddress( const char* ip, unsigned short port );
IP4Str    AddressToString( address_t addr );

socket_t CreateSocket( ESocket::Type type );
void     DestroySocket( socket_t sock );

bool      Connect    ( socket_t sock, address_t toAddr );
address_t IsConnected( socket_t sock );

bool     SendMsg( socket_t sock, const void* msg, unsigned msgSize );
unsigned GetMsgSize( socket_t sock );
unsigned GetMsg( void* msg, unsigned msgCapacity, socket_t sock );


}//
