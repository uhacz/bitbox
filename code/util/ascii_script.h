#pragma once

#include "containers.h"

////
//
/*
@ - create
$ - attribute
: - command
*/
struct bxAsciiScript_AttribData
{
    enum
    {
        eMAX_STRING_LEN = 255,
        eMAX_NUMBER_LEN = 64,
    };
    union
    {
        i32 inumber[eMAX_NUMBER_LEN];
        u32 unumber[eMAX_NUMBER_LEN];
        f32 fnumber[eMAX_NUMBER_LEN];
        char string[eMAX_STRING_LEN + 1];
    };
    u32 numBytes;

    int addNumberi( i32 n );
    int addNumberu( u32 n );
    int addNumberf( f32 n );
    int setString( const char* str, int len );

    void* dataPointer() const { return (void*)&fnumber[0]; }
    unsigned dataSizeInBytes() const { return numBytes; }
};

struct bxAsciiScript;
struct bxAsciiScript_Callback
{
    virtual void addCallback( bxAsciiScript* script ) = 0;

    virtual void onCreate( const char* typeName, const char* objectName ) = 0;
    virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData ) = 0;
    virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args ) = 0;
};

struct bxAsciiScript
{
    hashmap_t _map_callback;
};
namespace bxScene
{
    int  script_run( bxAsciiScript* script, const char* scriptTxt );
    void script_addCallback( bxAsciiScript* script, const char* name, bxAsciiScript_Callback* callback );
}///