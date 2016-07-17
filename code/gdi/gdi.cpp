#include "gdi.h"

namespace bx{
namespace gdi{
    namespace commands
    {
        const BackendDispatchFunction UploadCBuffer::DISPATCH_FUNCTION        = &dispatch::uploadCBuffer;
        const BackendDispatchFunction UploadBuffer::DISPATCH_FUNCTION         = &dispatch::uploadBuffer;
        const BackendDispatchFunction SetHwState::DISPATCH_FUNCTION           = &dispatch::setHwState;
        const BackendDispatchFunction SetTexture::DISPATCH_FUNCTION           = &dispatch::setTexture;
        const BackendDispatchFunction SetSampler::DISPATCH_FUNCTION           = &dispatch::setSampler;
        const BackendDispatchFunction SetCBuffer::DISPATCH_FUNCTION           = &dispatch::setCBuffer;
        const BackendDispatchFunction SetBufferRO::DISPATCH_FUNCTION          = &dispatch::setBufferRO;
        const BackendDispatchFunction Draw::DISPATCH_FUNCTION                 = &dispatch::draw;
        const BackendDispatchFunction DrawIndexed::DISPATCH_FUNCTION          = &dispatch::drawIndexed;
        const BackendDispatchFunction DrawInstanced::DISPATCH_FUNCTION        = &dispatch::drawInstanced;
        const BackendDispatchFunction DrawIndexedInstanced::DISPATCH_FUNCTION = &dispatch::drawIndexedInstanced;
    }///
    
    namespace dispatch
    {
        void _null_( bxGdiContext*, const void* )
        {
            //// nothing
        }

        void uploadCBuffer( bxGdiContext* ctx, const void* data )
        {
            const commands::UploadCBuffer* cmd = ( commands::UploadCBuffer* )data;
            ctx->backend()->updateCBuffer( cmd->cbuffer, cmd->memory );
        }

        void uploadBuffer( bxGdiContext* ctx, const void* data )
        {
            const commands::UploadBuffer* cmd = ( commands::UploadBuffer* )data;
            unsigned char* mapped = bxGdi::bufferMap( ctx->backend(), cmd->cbuffer, cmd->offset, cmd->size );
            memcpy( mapped, cmd->memory, cmd->size );
            ctx->backend()->unmap( cmd->cbuffer.rs );
        }

        void setHwState( bxGdiContext* ctx, const void* data )
        {
            const commands::SetHwState* cmd = ( commands::SetHwState* )data;
            ctx->setHwState( cmd->desc );
        }

        void setTexture( bxGdiContext* ctx, const void* data )
        {
            const commands::SetTexture* cmd = ( commands::SetTexture* )data;
            ctx->setTexture( cmd->texture, cmd->slot, cmd->stage_mask );
        }

        void setSampler( bxGdiContext* ctx, const void* data )
        {
            commands::SetSampler* cmd = ( commands::SetSampler* )data;
            ctx->setSampler( cmd->sampler, cmd->slot, cmd->stage_mask );
        }

        void setCBuffer( bxGdiContext* ctx, const void* data )
        {
            commands::SetCBuffer* cmd = ( commands::SetCBuffer* )data;
            ctx->setCbuffer( cmd->buffer, cmd->slot, cmd->stage_mask );
        }

        void setBufferRO( bxGdiContext* ctx, const void* data )
        {
            commands::SetBufferRO* cmd = ( commands::SetBufferRO* )data;
            ctx->setBufferRO( cmd->buffer, cmd->slot, cmd->stage_mask );
        }

        void draw( bxGdiContext* ctx, const void* data )
        {
            commands::Draw* cmd = ( commands::Draw* )data;
            ctx->setVertexBuffers( cmd->vbuffers, cmd->vbuffer_count );
            ctx->setTopology( cmd->topology );
            ctx->draw( cmd->vertex_count, cmd->vertex_start );
        }

        void drawIndexed( bxGdiContext* ctx, const void* data )
        {
            commands::DrawIndexed* cmd = ( commands::DrawIndexed* )data;
            ctx->setVertexBuffers( cmd->vbuffers, cmd->vbuffer_count );
            ctx->setIndexBuffer( cmd->ibuffer );
            ctx->setTopology( cmd->topology );
            ctx->drawIndexed( cmd->index_count, cmd->index_start, cmd->vertex_base );
        }

        void drawInstanced( bxGdiContext* ctx, const void* data )
        {
            commands::DrawInstanced* cmd = ( commands::DrawInstanced* )data;
            ctx->setVertexBuffers( cmd->vbuffers, cmd->vbuffer_count );
            ctx->setTopology( cmd->topology );
            ctx->drawInstanced( cmd->vertex_count, cmd->vertex_start, cmd->instance_count );
        }

        void drawIndexedInstanced( bxGdiContext* ctx, const void* data )
        {
            commands::DrawIndexedInstanced* cmd = ( commands::DrawIndexedInstanced* )data;
            ctx->setVertexBuffers( cmd->vbuffers, cmd->vbuffer_count );
            ctx->setIndexBuffer( cmd->ibuffer );
            ctx->setTopology( cmd->topology );
            ctx->drawIndexedInstanced( cmd->index_count, cmd->index_start, cmd->instance_count, cmd->vertex_base );
        }

    }///

}}///
