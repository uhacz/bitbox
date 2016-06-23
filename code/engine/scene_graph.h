#pragma once

#include <util/type.h>
#include <util/vectormath/vectormath.h>
#include <util/containers.h>

namespace bx
{
    struct TransformInstance
    { 
        u32 i; 
    };

    struct SceneGraph
    {
        SceneGraph( bxAllocator* alloc = bxDefaultAllocator() );
        ~SceneGraph();

        TransformInstance create( id_t id, const Matrix4& pose );
        TransformInstance create( id_t id, const Vector3& pos, const Quat& rot, const Vector3& scale );
        void destroy( TransformInstance i );

        TransformInstance get( id_t id );

        void setLocalPosition( TransformInstance i, const Vector3& pos );
        void setLocalRotation( TransformInstance i, const Quat& rot );
        void setLocalScale( TransformInstance i, const Vector3& scale );
        void setLocalPose( TransformInstance i, const Matrix4& pose );
        void setWorldPose( TransformInstance i, const Matrix4& pose );

        Vector3 localPosition( TransformInstance i ) const;
        Quat localRotation( TransformInstance i ) const;
        Vector3 localScale( TransformInstance i ) const;
        Matrix4 localPose( TransformInstance i ) const;
        Vector3 worldPosition( TransformInstance i ) const;
        Quat worldRotation( TransformInstance i ) const;
        Matrix4 worldPose( TransformInstance i ) const;

        u32 size() const;

        void link( TransformInstance child, TransformInstance parent );
        void unlink( TransformInstance child );

        void clearChanged();
        void getChanged( array_t<id_t>* units, array_t<Matrix4>* world_poses );

        bool isValid( TransformInstance i );
        void setLocal( TransformInstance i );
        void transform( const Matrix4& parent, TransformInstance i );

    private:
        void grow();
        void allocate( u32 num );
        TransformInstance makeInstance( u32 i )
        {
            TransformInstance ti = { i };
            return ti;
        }

        void unitDestroyedCallback( id_t id );
        static void unitDestroyedCallback( id_t id, void* user_ptr )
        {
            ( (SceneGraph*)user_ptr )->unitDestroyedCallback( id );
        }

    private:
        struct Pose
        {
            Vector3 pos = Vector3(0.f);
            Matrix3 rot = Matrix3::identity();
            Vector3 scale = Vector3( 1.f );

            Pose(){}
            Pose( const Matrix4& m4 );
            static Matrix4 toMatrix4( const Pose& pose );
        };

        struct InstanceData
        {
            u32 size = 0;
            u32 capacity = 0;
            void* buffer = nullptr;

            Matrix4* world = nullptr;
            Pose* local = nullptr;
            id_t* unit = nullptr;
            TransformInstance* parent = nullptr;
            TransformInstance* first_child = nullptr;
            TransformInstance* next_sibling = nullptr;
            TransformInstance* prev_sibling = nullptr;
            u8* changed = nullptr;
        };

        bxAllocator* _allocator = nullptr;
        InstanceData _data;
        hashmap_t _map;


    };
}///

