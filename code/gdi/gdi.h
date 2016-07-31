#pragma once

#include "gdi_context.h"
#include <util/linear_allocator.h>
#include <algorithm>
#include <util/buffer_utils.h>
#include <util/vectormath/vectormath.h>

namespace bx {
namespace gdi {

    typedef void( *BackendDispatchFunction )( bxGdiContext*, const void* );
    namespace commands
    {
        struct UploadCBuffer
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            bxGdiBuffer cbuffer;
            char* memory;
        };
        struct UploadBuffer
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            bxGdiBuffer cbuffer;
            char* memory;
            u32 offset;
            u32 size;
        };

        struct SetHwState
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            bxGdiHwStateDesc desc;
        };
        struct SetTexture
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u16 slot;
            u16 stage_mask;
            bxGdiTexture texture;
        };
        struct SetSampler
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u16 slot;
            u16 stage_mask;
            bxGdiSamplerDesc sampler;
        };
        struct SetCBuffer
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u16 slot;
            u16 stage_mask;
            bxGdiBuffer buffer;
        };
        struct SetBufferRO
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u16 slot;
            u16 stage_mask;
            bxGdiBuffer buffer;
        };

        struct Draw
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u32 vertex_count;
            u32 vertex_start;
            u32 topology;
            u32 vbuffer_count;
            bxGdiVertexBuffer vbuffers[1];
        };
        struct DrawIndexed
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;

            u32 index_count;
            u32 index_start;
            u32 vertex_base;
            u32 topology;
            u32 vbuffer_count;
            bxGdiIndexBuffer ibuffer;
            bxGdiVertexBuffer vbuffers[1];
        };
        struct DrawInstanced
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;

            u32 vertex_count;
            u32 vertex_start;
            u32 instance_count;
            u32 topology;
            u32 vbuffer_count;
            bxGdiVertexBuffer vbuffers[1];
        };
        struct DrawIndexedInstanced
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;

            u32 index_count;
            u32 index_start;
            u32 instance_count;
            u32 vertex_base;
            u32 topology;
            u32 vbuffer_count;
            bxGdiIndexBuffer ibuffer;
            bxGdiVertexBuffer vbuffers[1];
        };
    }////
    namespace dispatch
    {
        void _null_( bxGdiContext*, const void* );
        void uploadCBuffer( bxGdiContext* ctx, const void* data );
        void uploadBuffer( bxGdiContext* ctx, const void* data );
        void setHwState( bxGdiContext* ctx, const void* data );
        void setTexture( bxGdiContext* ctx, const void* data );
        void setSampler( bxGdiContext* ctx, const void* data );
        void setCBuffer( bxGdiContext* ctx, const void* data );
        void setBufferRO( bxGdiContext* ctx, const void* data );
        void draw( bxGdiContext* ctx, const void* data );
        void drawIndexed( bxGdiContext* ctx, const void* data );
        void drawInstanced( bxGdiContext* ctx, const void* data );
        void drawIndexedInstanced( bxGdiContext* ctx, const void* data );
    }////

    typedef void* CommandPacket;
    namespace command_packet
    {
        static const u32 OFFSET_NEXT_COMMAND_PACKET = 0;
        static const u32 OFFSET_DISPATCH_FUNCTION = OFFSET_NEXT_COMMAND_PACKET + sizeof( CommandPacket );
        static const u32 OFFSET_COMMAND = OFFSET_DISPATCH_FUNCTION + sizeof( BackendDispatchFunction );

        template <typename T>
        inline u32 size( u32 auxMemorySize )
        {
            return OFFSET_COMMAND + sizeof( T ) + auxMemorySize;
        };

        template <typename T>
        inline CommandPacket create( size_t auxMemorySize, bx::LinearAllocator* allocator )
        {
            return (CommandPacket)allocator->alloc( size<T>( auxMemorySize ), ALIGNOF( T ) );
            //return ::operator new( size<T>( auxMemorySize ) );
        }

        inline CommandPacket* nextCommandPacket( CommandPacket packet )
        {
            return (CommandPacket*)( (char*)(packet)+OFFSET_NEXT_COMMAND_PACKET );
        }

        template <typename T>
        inline CommandPacket* nextCommandPacket( T* command )
        {
            return (CommandPacket*)( (char*)(command)-OFFSET_COMMAND + OFFSET_NEXT_COMMAND_PACKET );
        }

        inline BackendDispatchFunction* backendDispatchFunction( CommandPacket packet )
        {
            return (BackendDispatchFunction*)( (char*)(packet)+OFFSET_DISPATCH_FUNCTION );
        }

        template <typename T>
        inline T* command( CommandPacket packet )
        {
            return (T*)( (char*)(packet)+OFFSET_COMMAND );
        }

        template <typename T>
        inline char* auxiliaryMemory( T* command )
        {
            return (char*)(command)+sizeof( T );
        }

        inline void nextCommandPacketStore( CommandPacket packet, CommandPacket nextPacket )
        {
            *command_packet::nextCommandPacket( packet ) = nextPacket;
        }

        template <typename T>
        void nextCommandPacketStore( T* command, CommandPacket nextPacket )
        {
            *command_packet::nextCommandPacket<T>( command ) = nextPacket;
        }

        inline void dispatchFunctionStore( CommandPacket packet, BackendDispatchFunction dispatchFunction )
        {
            *command_packet::backendDispatchFunction( packet ) = dispatchFunction;
        }

        inline const CommandPacket nextCommandPacketLoad( const CommandPacket packet )
        {
            return *nextCommandPacket( packet );
        }

        inline const BackendDispatchFunction dispatchFunctionLoad( const  CommandPacket packet )
        {
            return *backendDispatchFunction( packet );
        }

        inline const void* commandLoad( const CommandPacket packet )
        {
            return (char*)(packet)+OFFSET_COMMAND;
        }

        void submit( bxGdiContext* ctx, const CommandPacket packet )
        {
            const BackendDispatchFunction function = dispatchFunctionLoad( packet );
            const void* command = commandLoad( packet );
            function( ctx, command );
        }

    }////

    template< typename T >
    struct BIT_ALIGNMENT_16 CommandBucket
    {
        typedef T Key;

        template< typename U >
        U* commandAdd( Key key, u32 auxMemorySize, const Matrix4* worldMatrix = nullptr )
        {
            CommandPacket packet = command_packet::create<U>( auxMemorySize, &_allocator );

            const u32 index = _size++;
            SYS_ASSERT( index < _capacity );
            key.indexSet( index );
            _keys[index] = key;

            if( worldMatrix )
            {
                const u32 worldMatrixIndex = _world_matrix_size++;
                _world_matrix[worldMatrixIndex] = *worldMatrix;
                _data[index] = worldMatrixPacket;

                CommandPacket worldMatrixPacket = command_packet::create<commands::UploadCBuffer>( sizeof(u32), &_allocator );
                command_packet::nextCommandPacketStore( worldMatrixPacket, packet );
                command_packet::dispatchFunctionStore( packet, commands::UploadCBuffer::DISPATCH_FUNCTION );

                commands::UploadCBuffer* uploadCmd = command_packet::command<commands::UploadCBuffer>( worldMatrixPacket );
                u32* uploadCmdData = command_packet::auxiliaryMemory( uploadCmd );
                uploadCmdData[0] = worldMatrixIndex;
                uploadCmd->cbuffer = _world_matrix_index_cbuffer;
                uploadCmd->memory = (char*)uploadCmdData;

            }
            else
            {
                _data[index] = packet;
            }

            command_packet::nextCommandPacketStore( packet, nullptr );
            command_packet::dispatchFunctionStore( packet, U::DISPATCH_FUNCTION );
            return command_packet::command<U>( packet );
        }

        template< typename U, typename V >
        U* commandAppend( V* command, u32 auxMemorySize )
        {
            CommandPacket packet = command_packet::create<U>( auxMemorySize, &_allocator );
            
            // append this packet to given one
            command_packet::nextCommandPacketStore<V>( command, packet );

            command_packet::nextCommandPacketStore( packet, nullptr );
            command_packet::dispatchFunctionStore( packet, U::DISPATCH_FUNCTION );
            return command_packet::command<U>( packet );
        }

        void sort()
        {
            std::sort( _keys, _keys + _size, std::less<Key>() );
        }
        void submit( bxGdiContext* ctx )
        {
            for( u32 i = 0; i < _size; ++i )
            {
                CommandPacket packet = _data[i];
                do 
                {
                    command_packet::submit( ctx, packet );
                    packet = command_packet::nextCommandPacketLoad( packet );
                } while ( packet != nullptr );
            }
        }

        static CommandBucket<T>* create( bxGdiDeviceBackend* device, u32 capacity, u32 auxMemoryPool, bxAllocator* allocator  = bxDefaultAllocator() )
        {
            u32 memSize = sizeof( CommandBucket<T> );
            memSize += capacity * ( sizeof( Key ) + sizeof( CommandPacket ) );
            memSize += capacity * ( sizeof( Matrix4 ) );
            memSize += auxMemoryPool;

            void* mem = BX_MALLOC( allocator, memSize, 16 );
            memset( mem, 0x00, memSize );

            bxBufferChunker chunker( mem, memSize );

            CommandBucket<T>* bucket = chunker.add< CommandBucket<T> >();
            bucket->_world_matrix = chunker.add<Matrix4>( capacity );
            bucket->_keys = chunker.add<Key>( capacity );
            bucket->_data = chunker.add<void*>( capacity );
            void* auxMemStart = chunker.addBlock( auxMemoryPool );
            void* auxMemEnd = chunker.current;
            chunker.check();

            bucket->_capacity = capacity;

            bucket->_allocator = bx::LinearAllocator( auxMemStart, auxMemEnd );
            bucket->_main_allocator = allocator;

            bucket->_world_matrix_buffer = device->createBuffer( capacity, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
            bucket->_world_matrix_it_buffer = device->createBuffer( capacity, bxGdiFormat( bxGdi::eTYPE_FLOAT, 4 ), bxGdi::eBIND_SHADER_RESOURCE, bxGdi::eCPU_WRITE, bxGdi::eGPU_READ );
            bucket->_world_matrix_index_cbuffer = device->createConstantBuffer( 16 );
            
            return bucket;
        }
        static void destroy( bxGdiDeviceBackend* device, CommandBucket<T>** bucket )
        {
            if( !bucket[0] )
                return;

            device->releaseBuffer( &bucket[0]->_world_matrix_index_cbuffer );
            device->releaseBuffer( &bucket[0]->_world_matrix_it_buffer );
            device->releaseBuffer( &bucket[0]->_world_matrix_buffer );

            bxAllocator* allocator = bucket[0]->_main_allocator;
            BX_FREE0( allocator, bucket[0] );
        }
        
    private:
        Matrix4* _world_matrix = nullptr;
        Key* _keys = nullptr;
        void** _data = nullptr;

        u32 _capacity = 0;
        u32 _size = 0;
        u32 _world_matrix_size = 0;

        bx::LinearAllocator _allocator;
        bxAllocator* _main_allocator = nullptr;

        bxGdiBuffer _world_matrix_buffer;
        bxGdiBuffer _world_matrix_it_buffer;
        bxGdiBuffer _world_matrix_index_cbuffer;
    };

}}////

