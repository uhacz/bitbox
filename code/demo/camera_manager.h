#pragma once

namespace bx
{
    struct GfxCamera;
    struct GfxContext;

    struct CameraStack;
    int cameraStackPush( CameraStack* s, GfxCamera* camera );
    void cameraStackPop( CameraStack* s, int count = 1 );
    GfxCamera* cameraStackTop( CameraStack* s );

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct CameraManager
    {
        int add( GfxCamera* camera, const char* name );
        void remove( GfxCamera* camera );
        void removeByName( const char* name );

        GfxCamera* find( const char* name );
        CameraStack* stack();
    };

    void cameraManagerStartup( CameraManager** cm, GfxContext* gfx );
    void cameraManagerShutdown( CameraManager** cm );

}////

#include <util/ascii_script.h>
namespace bx
{
    struct CameraManagerSceneScriptCallback : public bxAsciiScript_Callback
    {
        virtual void onCreate( const char* typeName, const char* objectName );
        virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData );
        virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args );

        CameraManagerSceneScriptCallback();

        GfxContext* _gfx;
        CameraManager* _menago;
        GfxCamera* _current;


    };
}////
