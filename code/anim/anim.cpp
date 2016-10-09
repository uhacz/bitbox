#include "anim.h"
#include <util/memory.h>

namespace bxAnim
{
bxAnim_Context* contextInit( const bxAnim_Skel& skel )
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
    ctx->numJoints = skel.numJoints;

    for( u32 i = 0; i < bxAnim_Context::ePOSE_STACK_SIZE; ++i )
    {
        bxAnim_Joint* joints = ctx->poseStack[i];
        for( u32 j = 0; j < skel.numJoints; ++j )
        {
            joints[j] = bxAnim_Joint::identity();
        }
    }

	return ctx;
}

void contextDeinit( bxAnim_Context** ctx )
{
	BX_FREE0( bxDefaultAllocator(), ctx[0] );
}

}///

#include <resource_manager/resource_manager.h>
namespace bxAnimExt
{
    //static uptr _LoadResource( bx::ResourceManager* resourceManager, const char* relativePath )
    //{
    //    bx::ResourceID resourceId = bx::ResourceManager::createResourceID( relativePath );
    //    uptr resourceData = resourceManager->lookup( resourceId );
    //    if ( resourceData )
    //    {
    //        resourceManager->referenceAdd( resourceId );
    //    }
    //    else
    //    {
    //        bxFS::File file = resourceManager->readFileSync( relativePath );
    //        if ( file.ok() )
    //        {
    //            resourceData = uptr( file.bin );
    //            resourceManager->insert( resourceId, resourceData );
    //        }
    //    }
    //    return resourceData;
    //}
    //static void _UnloadResource( bx::ResourceManager* resourceManager, uptr resourceData )
    //{
    //    bx::ResourceID resourceId = resourceManager->find( resourceData );
    //    if ( !resourceId )
    //    {
    //        bxLogError( "resource not found!" );
    //        return;
    //    }

    //    int refLeft = resourceManager->referenceRemove( resourceId );
    //    if ( refLeft == 0 )
    //    {
    //        void* ptr = (void*)resourceData;
    //        BX_FREE( bxDefaultAllocator(), ptr );
    //    }
    //}

    bxAnim_Skel* loadSkelFromFile( bx::ResourceManager* resourceManager, const char* relativePath )
    {
        bx::ResourceLoadResult load_result = resourceManager->loadResource( relativePath, bx::EResourceFileType::BINARY );
        return (bxAnim_Skel*)load_result.ptr;
        //uptr resourceData = _LoadResource( resourceManager, relativePath );
        //return (bxAnim_Skel*)resourceData;
    }

    bxAnim_Clip* loadAnimFromFile( bx::ResourceManager* resourceManager, const char* relativePath )
    {
        bx::ResourceLoadResult load_result = resourceManager->loadResource( relativePath, bx::EResourceFileType::BINARY );
        return (bxAnim_Clip*)load_result.ptr;

        //uptr resourceData = _LoadResource( resourceManager, relativePath );
        //return (bxAnim_Clip*)resourceData;
    }

    void unloadSkelFromFile( bx::ResourceManager* resourceManager, bxAnim_Skel** skel )
    {
        if ( !skel[0] )
            return;

        resourceManager->unloadResource( (bx::ResourcePtr*)skel );
        //_UnloadResource( resourceManager, uptr( skel[0] ) );
        //skel[0] = 0;
    }

    void unloadAnimFromFile( bx::ResourceManager* resourceManager, bxAnim_Clip** clip )
    {
        if( !clip[0] )
            return;
    
        resourceManager->unloadResource( ( bx::ResourcePtr* )clip );
        //_UnloadResource( resourceManager, uptr( clip[0] ) );
        //clip[0] = 0;
    }

    void localJointsToWorldJoints( bxAnim_Joint* outJoints, const bxAnim_Joint* inJoints, const bxAnim_Skel* skel, const bxAnim_Joint& rootJoint )
    {
        const u16* parentIndices = TYPE_OFFSET_GET_POINTER( const u16, skel->offsetParentIndices );
        bxAnim::localJointsToWorldJoints( outJoints, inJoints, parentIndices, skel->numJoints, rootJoint );
    }

    void localJointsToWorldMatrices( Matrix4* outMatrices, const bxAnim_Joint* inJoints, const bxAnim_Skel* skel, const bxAnim_Joint& rootJoint )
    {
        const u16* parentIndices = TYPE_OFFSET_GET_POINTER( const u16, skel->offsetParentIndices );
        bxAnim::localJointsToWorldMatrices4x4( outMatrices, inJoints, parentIndices, skel->numJoints, rootJoint );
    }
    
    void processBlendTree( bxAnim_Context* ctx, const u16 root_index, const bxAnim_BlendBranch* blend_branches, unsigned int num_branches, const bxAnim_BlendLeaf* blend_leaves, unsigned int num_leaves )
    {
        bxAnim::evaluateBlendTree( ctx, root_index, blend_branches, num_branches, blend_leaves, num_leaves );
        bxAnim::evaluateCommandList( ctx, blend_branches, num_branches, blend_leaves, num_leaves );
    }

}///