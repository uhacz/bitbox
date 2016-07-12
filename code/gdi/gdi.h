#pragma once

#include "gdi_context.h"
#include <util/linear_allocator.h>

namespace bx {
namespace gdi {

    typedef void( *BackendDispatchFunction )( bxGdiContext* ctx, const void* );
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
        struct SetTextures
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u32 count;
            bxGdiTexture textures[1];
        };
        struct SetSamplers
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u32 count;
            bxGdiSamplerDesc samplers[1];
        };
        struct SetCBuffers
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u32 count;
            bxGdiBuffer buffers[1];
        };
        struct SetBuffers
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;
            u32 count;
            bxGdiBuffer buffers[1];
        };

        struct Draw
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;

            u32 vertex_count;
            u32 vertex_start;

            u32 vbuffer_count;
            bxGdiIndexBuffer ibuffer;
            bxGdiVertexBuffer vbuffers[1];
        };
        struct DrawIndexed
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;

            u32 index_count;
            u32 index_start;
            u32 vertex_base;

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

            u32 vbuffer_count;
            bxGdiIndexBuffer ibuffer;
            bxGdiVertexBuffer vbuffers[1];
        };
        struct DrawIndexedInstanced
        {
            static const BackendDispatchFunction DISPATCH_FUNCTION;

            u32 index_count;
            u32 index_start;
            u32 instance_count;
            u32 vertex_base;

            u32 vbuffer_count;
            bxGdiIndexBuffer ibuffer;
            bxGdiVertexBuffer vbuffers[1];
        };
    }////
    namespace dispatch
    {
        void _null_( bxGdiContext*, const void* )
        {
            //// nothing
        }

        //////////////////////////////////////////////////////////////////////////
        void uploadCBuffer( bxGdiContext* ctx, const void* data )
        {
            const commands::UploadCBuffer* cmd = ( commands::UploadCBuffer* )data;
            ctx->backend()->updateCBuffer( cmd->cbuffer, memory );
        }
        commands::UploadCBuffer::DISPATCH_FUNCTION = &uploadCBuffer;

        //////////////////////////////////////////////////////////////////////////
        void uploadBuffer( bxGdiContext* ctx, const void* data )
        {
            const commands::UploadBuffer* cmd = ( commands::UploadBuffer* )data;
            unsigned char* mapped = bxGdi::bufferMap( ctx->backend(), cmd->cbuffer, cmd->offset, cmd->size );
            memcpy( mapped, cmd->memory, cmd->size );
            ctx->->backend()->unmap( cmd->cbuffer.rs );
        }
        commands::UploadBuffer::DISPATCH_FUNCTION = &uploadBuffer;

        //////////////////////////////////////////////////////////////////////////
        void setHwState( bxGdiContext* ctx, const void* data )
        {
            const commands::SetHwState* cmd = ( commands::SetHwState* )data;
            ctx->setHwState( cmd->desc );
        }
        commands::SetHwState::DISPATCH_FUNCTION = &setHwState;

        //////////////////////////////////////////////////////////////////////////
        void setTextures( bxGdiContext* ctx, const void* data )
        {
            
        }
        void setSamplers( bxGdiContext* ctx, const void* data )
        {}
        void setCBuffers( bxGdiContext* ctx, const void* data )
        {}
        void setBuffers( bxGdiContext* ctx, const void* data )
        {}
        void draw( bxGdiContext* ctx, const void* data )
        {}
        void drawIndexed( bxGdiContext* ctx, const void* data )
        {}
        void drawInstanced( bxGdiContext* ctx, const void* data )
        {}
        void drawIndexedInstanced( bxGdiContext* ctx, const void* data )
        {}
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

        inline const BackendDispatchFunction backendDispatchFunctionLoad( const  CommandPacket packet )
        {
            return *backendDispatchFunction( packet );
        }

        inline const void* commandLoad( const CommandPacket packet )
        {
            return (char*)(packet)+OFFSET_COMMAND;
        }

    }////

    template< typename T >
    struct CommandBucket
    {
        typedef T Key;

        template< typename U >
        U* commandAdd( Key key, u32 auxMemorySize )
        {
            CommandPacket packet = command_packet::create<U>( auxMemorySize );

            {
                const u32 index = _size++;
                SYS_ASSERT( index < _capacity );
                _keys[index] = key;
                _data[index] = packet;
            }

            command_packet::nextCommandPacketStore( packet, nullptr );
            command_packet::dispatchFunctionStore( packet, U::DISPATCH_FUNCTION );

            return command_packet::command<U>( packet );
        }

        void sort();
        void submit();

    private:

    private:
        Key* _keys = nullptr;
        void** _data = nullptr;

        u32 _capacity = 0;
        u32 _size = 0;

        bx::LinearAllocator _allocator;
    };

}}////

