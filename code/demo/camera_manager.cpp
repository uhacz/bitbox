#include "camera_manager.h"
#include <util/array.h>
#include <util/memory.h>
#include <util/string_util.h>
#include <util/debug.h>
#include <string.h>
#include <gfx/gfx.h>

namespace bx
{ 
    struct CameraStackImpl : public CameraStack
    {
        array_t< GfxCamera* > _camera;
    };

    CameraStackImpl* implGet( CameraStack* cs ) { return (CameraStackImpl*)cs; }

    int CameraStack::push( GfxCamera* camera )
    {
        return array::push_back( implGet( this )->_camera, camera );
    }

    void CameraStack::pop( int count /*= 1 */ )
    {
        CameraStackImpl* impl = implGet( this );
        while ( count && !array::empty( impl->_camera ) )
        {
            array::pop_back( impl->_camera );
            --count;
        }
    }
    GfxCamera* CameraStack::top()
    {
        CameraStackImpl* impl = implGet( this );
        return array::empty( impl->_camera ) ? nullptr : array::back( impl->_camera );
    }

    void cameraStackRemove( CameraStackImpl* s, GfxCamera* camera )
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
        CameraStackImpl _stack;

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

        void _Clear()
        {
            while( array::size( _data ) )
            {
                int lastIndex = array::size( _data ) - 1;
                GfxCamera* camera = _data[lastIndex].camera;
                _Remove( lastIndex );
                gfxCameraDestroy( &camera );
            }
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
        impl->_Clear();
        BX_DELETE( bxDefaultAllocator(), impl );

        cm[0] = nullptr;
    }

}////

namespace bx
{
    bool gfxCameraSetAttribute( GfxCamera* camera, const char* attribName, const void* data, int dataSize )
    {
        if( string::equal( "pos", attribName ) )
        {
            if( dataSize != 12 )
                goto label_cameraAttributeInvalidSize;

            const float* xyz = (float*)data;
            Matrix4 world = gfxCameraWorldMatrixGet( camera );
            world.setTranslation( Vector3( xyz[0], xyz[1], xyz[2] ) );
            gfxCameraWorldMatrixSet( camera, world );
        }
        else if( string::equal( "rot", attribName ) )
        {
            if( dataSize != 12 )
                goto label_cameraAttributeInvalidSize;
            const float* xyz = (float*)data;
            const Vector3 eulerXYZ( xyz[0], xyz[1], xyz[2] );

            Matrix4 world = gfxCameraWorldMatrixGet( camera );
            world.setUpper3x3( Matrix3::rotationZYX( eulerXYZ ) );
            gfxCameraWorldMatrixSet( camera, world );
        }
        else if( string::equal( "zNear", attribName ) )
        {
            if( dataSize != 4 )
                goto label_cameraAttributeInvalidSize;

            const float value = *(float*)data;
            GfxCameraParams params = gfxCameraParamsGet( camera );
            params.zNear = value;
            gfxCameraParamsSet( camera, params );
        }
        else if( string::equal( "zFar", attribName ) )
        {
            if( dataSize != 4 )
                goto label_cameraAttributeInvalidSize;

            const float value = *(float*)data;
            GfxCameraParams params = gfxCameraParamsGet( camera );
            params.zFar = value;
            gfxCameraParamsSet( camera, params );
        }
        else
        {
            bxLogError( "camera attribute '%s' not found", attribName );
        }

        return true;

    label_cameraAttributeInvalidSize:
        bxLogError( "invalid data size" );
        return false;
    }


    CameraManagerSceneScriptCallback::CameraManagerSceneScriptCallback()
        : _gfx(nullptr)
        , _menago( nullptr )
        , _current( nullptr )
    {}

    void CameraManagerSceneScriptCallback::onCreate( const char* typeName, const char* objectName )
    {
        gfxCameraCreate( &_current, _gfx );
        _menago->add( _current, objectName );
    }

    void CameraManagerSceneScriptCallback::onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData )
    {
        if( _current )
        {
            gfxCameraSetAttribute( _current, attrName, attribData.dataPointer(), attribData.dataSizeInBytes() );
        }
    }

    void CameraManagerSceneScriptCallback::onCommand( const char* cmdName, const bxAsciiScript_AttribData& args )
    {
        if( string::equal( "camera_push", cmdName ) )
        {
            GfxCamera* camera = _menago->find( args.string );
            if( camera )
            {
                _menago->stack()->push( camera );
            }
        }
    }


}///