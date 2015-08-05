#include "anim.h"
#include <util/memory.h>

namespace bxAnim
{
bxAnim_Context* context_init( const bxAnim_Skel& skel )
{
	const u32 poseMemorySize = skel.numJoints * sizeof( bxAnim_Joint );
	
	u32 memSize = 0;
	memSize += sizeof( bxAnim_Context );
	memSize += poseMemorySize * bxAnim_Context::ePOSE_CACHE_SIZE;
	memSize += poseMemorySize * bxAnim_Context::ePOSE_STACK_SIZE;
	memSize += sizeof( bxAnim_Cmd ) * bxAnim_Context::eCMD_ARRAY_SIZE;

	u8* memory = (u8*)BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
	memset( memory, 0, memSize );

	bxAnim_Context* ctx = (bxAnim_Context*)memory;

	u8* current_pointer = memory + sizeof(bxAnim_Context);
	for( u32 i = 0; i < bxAnim_Context::ePOSE_CACHE_SIZE; ++i )
	{
		ctx->poseCache[i] = (bxAnim_Joint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	for( u32 i = 0; i < bxAnim_Context::ePOSE_STACK_SIZE; ++i )
	{
		ctx->poseStack[i] = (bxAnim_Joint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	ctx->cmdArray = (bxAnim_Cmd*)current_pointer;
	current_pointer += sizeof(bxAnim_Cmd) * bxAnim_Context::eCMD_ARRAY_SIZE;
	SYS_ASSERT( (iptr)current_pointer == (iptr)( memory + memSize ) );

	return ctx;
}

void context_deinit( bxAnim_Context** ctx )
{
	BX_FREE0( bxDefaultAllocator(), ctx[0] );
}

}///

#include <resource_manager/resource_manager.h>
namespace bxAnimExt
{
    static uptr _LoadResource( bxResourceManager* resourceManager, const char* relativePath )
    {
        bxResourceID resourceId = bxResourceManager::createResourceID( relativePath );
        uptr resourceData = resourceManager->lookup( resourceId );
        if ( resourceData )
        {
            resourceManager->referenceAdd( resourceId );
        }
        else
        {
            bxFS::File file = resourceManager->readFileSync( relativePath );
            if ( file.ok() )
            {
                resourceData = uptr( file.bin );
                resourceManager->insert( resourceId, resourceData );
            }
        }
        return resourceData;
    }
    static void _UnloadResource( bxResourceManager* resourceManager, uptr resourceData )
    {
        bxResourceID resourceId = resourceManager->find( resourceData );
        if ( !resourceId )
        {
            bxLogError( "resource not found!" );
            return;
        }

        int refLeft = resourceManager->referenceRemove( resourceId );
        if ( refLeft == 0 )
        {
            void* ptr = (void*)resourceData;
            BX_FREE( bxDefaultAllocator(), ptr );
        }
    }

    bxAnim_Skel* loadSkelFromFile( bxResourceManager* resourceManager, const char* relativePath )
    {
        uptr resourceData = _LoadResource( resourceManager, relativePath );
        return (bxAnim_Skel*)resourceData;
    }

    bxAnim_Clip* loadAnimFromFile( bxResourceManager* resourceManager, const char* relativePath )
    {
        uptr resourceData = _LoadResource( resourceManager, relativePath );
        return (bxAnim_Clip*)resourceData;
    }

    void unloadSkelFromFile( bxResourceManager* resourceManager, bxAnim_Skel** skel )
    {
        if ( !skel[0] )
            return;

        _UnloadResource( resourceManager, uptr( skel[0] ) );
        skel[0] = 0;
    }

    void unloadAnimFromFile( bxResourceManager* resourceManager, bxAnim_Clip** clip )
    {
        if( !clip[0] )
            return;
    
        _UnloadResource( resourceManager, uptr( clip[0] ) );
        clip[0] = 0;
    }

}///