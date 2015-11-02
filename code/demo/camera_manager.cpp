#include "camera_manager.h"
#include <util/array.h>
#include <util/memory.h>
#include <util/string_util.h>
#include <util/debug.h>
#include <string.h>

namespace bx
{
    struct CameraStack
    {
        array_t< GfxCamera* > _camera;
    };

    int cameraStackPush( CameraStack* s, GfxCamera* camera )
    {
        return array::push_back( s->_camera, camera );
    }

    void cameraStackPop( CameraStack* s, int count /*= 1 */ )
    {
        while ( count && !array::empty( s->_camera ) )
        {
            array::pop_back( s->_camera );
            --count;
        }
    }

    void cameraStackRemove( CameraStack* s, GfxCamera* camera )
    {
        for( int i = 0; i < array::size( s->_camera ); ++i )
        {
            if( camera == s->_camera[i] )
            {
                array::erase( s->_camera, i );
                break;
            }
        }
    }

    GfxCamera* cameraStackTop( CameraStack* s )
    {
        return array::empty( s->_camera ) ? nullptr : array::back( s->_camera );
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    struct CameraManagerImpl : public CameraManager
    {
        enum
        {
            eMAX_NAME_LEN = 64 - sizeof( void* ) - 1,
        };
        struct Entry
        {
            GfxCamera* camera;
            char name[eMAX_NAME_LEN + 1];
        };
        
        array_t< Entry > _data;
        CameraStack _stack;

        int _Add( GfxCamera* camera, const char* name )
        {
            SYS_ASSERT( string::length( name ) <= eMAX_NAME_LEN );
            
            int index = _FindByName( name );
            if( index != -1 )
            {
                return index;
            }

            Entry e;
            e.camera = camera;
            strcpy_s( e.name, eMAX_NAME_LEN, name );

            return array::push_back( _data, e );
        }
        
        void _Remove( int index )
        {
            if ( index == -1 || index >= array::size( _data ) )
                return;

            Entry& e = _data[index];
            cameraStackRemove( &_stack, e.camera );
            e.camera = 0;
            e.name[0] = 0;

            array::erase_swap( _data, index );
        }
        
        int _FindByName( const char* name )
        {
            for( int i = 0; i < array::size( _data ); ++i )
            {
                if( string::equal( name, _data[i].name ) )
                {
                    return i;
                }
            }
            return -1;
        }
        int _FindByPointer( GfxCamera* camera )
        {
            for ( int i = 0; i < array::size( _data ); ++i )
            {
                if( camera == _data[i].camera )
                {
                    return i;
                }
            }
            return -1;
        }

    };

    inline CameraManagerImpl* implGet( CameraManager* cm ) { return (CameraManagerImpl*)cm; }
    inline const CameraManagerImpl* implGet( const CameraManager* cm ) { return (CameraManagerImpl*)cm; }

    int CameraManager::add( GfxCamera* camera, const char* name )
    {
        CameraManagerImpl* impl = implGet( this );
        return impl->_Add( camera, name );
    }

    void CameraManager::remove( GfxCamera* camera )
    {
        CameraManagerImpl* impl = implGet( this );
        int index = impl->_FindByPointer( camera );
        impl->_Remove( index );
    }

    void CameraManager::removeByName( const char* name )
    {
        CameraManagerImpl* impl = implGet( this );
        int index = impl->_FindByName( name );
        impl->_Remove( index );
    }

    GfxCamera* CameraManager::find( const char* name )
    {
        CameraManagerImpl* impl = implGet( this );
        int index = impl->_FindByName( name );
        return (index == -1) ? nullptr : impl->_data[index].camera;
    }

    CameraStack* CameraManager::stack()
    {
        CameraManagerImpl* impl = implGet( this );
        return &impl->_stack;
    }

    void cameraManagerStartup( CameraManager** cm, GfxContext* gfx )
    {
        CameraManagerImpl* impl = BX_NEW( bxDefaultAllocator(), CameraManagerImpl );
        cm[0] = impl;
    }

    void cameraManagerShutdown( CameraManager** cm )
    {
        CameraManagerImpl* impl = implGet( cm[0] );
        BX_DELETE( bxDefaultAllocator(), impl );

        cm[0] = nullptr;
    }

}////

namespace bx
{

    GfxCameraManager_SceneScriptCallback::GfxCameraManager_SceneScriptCallback()
        : _gfx(nullptr)
        , _menago( nullptr )
        , _current( nullptr )
    {}

    void GfxCameraManager_SceneScriptCallback::onCreate( const char* typeName, const char* objectName )
    {

    }

    void GfxCameraManager_SceneScriptCallback::onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData )
    {

    }

    void GfxCameraManager_SceneScriptCallback::onCommand( const char* cmdName, const bxAsciiScript_AttribData& args )
    {

    }


}///