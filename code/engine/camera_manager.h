#pragma once

namespace bx
{
    struct GfxCamera;
    struct GfxContext;

    struct CameraStack
    {
        int push( GfxCamera* camera );
        void pop( int count = 1 );
        GfxCamera* top();
    };
    
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
        virtual void addCallback( bxAsciiScript* script ) override;
        virtual void onCreate( const char* typeName, const char* objectName ) override;
        virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData ) override;
        virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args ) override;

        GfxContext* _gfx = nullptr;
        CameraManager* _menago = nullptr;
        GfxCamera* _current = nullptr;
    };
}////
