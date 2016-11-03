#include "anim.h"
#include <util/memory.h>

namespace bx{ namespace anim {

Context* contextInit( const Skel& skel )
{
	const u32 poseMemorySize = skel.numJoints * sizeof( Joint );
	
	u32 memSize = 0;
	memSize += sizeof( Context );
	memSize += poseMemorySize * Context::ePOSE_CACHE_SIZE;
	memSize += poseMemorySize * Context::ePOSE_STACK_SIZE;
	memSize += sizeof( Cmd ) * Context::eCMD_ARRAY_SIZE;

	u8* memory = (u8*)BX_MALLOC( bxDefaultAllocator(), memSize, 16 );
	memset( memory, 0, memSize );

	Context* ctx = (Context*)memory;

	u8* current_pointer = memory + sizeof(Context);
	for( u32 i = 0; i < Context::ePOSE_CACHE_SIZE; ++i )
	{
		ctx->poseCache[i] = (Joint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	for( u32 i = 0; i < Context::ePOSE_STACK_SIZE; ++i )
	{
		ctx->poseStack[i] = (Joint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	ctx->cmdArray = (Cmd*)current_pointer;
	current_pointer += sizeof(Cmd) * Context::eCMD_ARRAY_SIZE;
	SYS_ASSERT( (iptr)current_pointer == (iptr)( memory + memSize ) );
    ctx->numJoints = skel.numJoints;

    for( u32 i = 0; i < Context::ePOSE_STACK_SIZE; ++i )
    {
        Joint* joints = ctx->poseStack[i];
        for( u32 j = 0; j < skel.numJoints; ++j )
        {
            joints[j] = Joint::identity();
        }
    }

	return ctx;
}

void contextDeinit( Context** ctx )
{
	BX_FREE0( bxDefaultAllocator(), ctx[0] );
}

}}///

#include <resource_manager/resource_manager.h>

namespace bx{ 
using namespace anim;
    
namespace anim_ext{

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

    Skel* loadSkelFromFile( bx::ResourceManager* resourceManager, const char* relativePath )
    {
        bx::ResourceLoadResult load_result = resourceManager->loadResource( relativePath, bx::EResourceFileType::BINARY );
        return (Skel*)load_result.ptr;
        //uptr resourceData = _LoadResource( resourceManager, relativePath );
        //return (Skel*)resourceData;
    }

    Clip* loadAnimFromFile( bx::ResourceManager* resourceManager, const char* relativePath )
    {
        bx::ResourceLoadResult load_result = resourceManager->loadResource( relativePath, bx::EResourceFileType::BINARY );
        return (Clip*)load_result.ptr;

        //uptr resourceData = _LoadResource( resourceManager, relativePath );
        //return (Clip*)resourceData;
    }

    void unloadSkelFromFile( bx::ResourceManager* resourceManager, Skel** skel )
    {
        if ( !skel[0] )
            return;

        resourceManager->unloadResource( (bx::ResourcePtr*)skel );
        //_UnloadResource( resourceManager, uptr( skel[0] ) );
        //skel[0] = 0;
    }

    void unloadAnimFromFile( bx::ResourceManager* resourceManager, Clip** clip )
    {
        if( !clip[0] )
            return;
    
        resourceManager->unloadResource( ( bx::ResourcePtr* )clip );
        //_UnloadResource( resourceManager, uptr( clip[0] ) );
        //clip[0] = 0;
    }

    void localJointsToWorldJoints( Joint* outJoints, const Joint* inJoints, const Skel* skel, const Joint& rootJoint )
    {
        const u16* parentIndices = TYPE_OFFSET_GET_POINTER( const u16, skel->offsetParentIndices );
        localJointsToWorldJoints( outJoints, inJoints, parentIndices, skel->numJoints, rootJoint );
    }

    void localJointsToWorldMatrices( Matrix4* outMatrices, const Joint* inJoints, const Skel* skel, const Joint& rootJoint )
    {
        const u16* parentIndices = TYPE_OFFSET_GET_POINTER( const u16, skel->offsetParentIndices );
        localJointsToWorldMatrices4x4( outMatrices, inJoints, parentIndices, skel->numJoints, rootJoint );
    }
    
    void processBlendTree( Context* ctx, const u16 root_index, const BlendBranch* blend_branches, unsigned int num_branches, const BlendLeaf* blend_leaves, unsigned int num_leaves )
    {
        evaluateBlendTree( ctx, root_index, blend_branches, num_branches, blend_leaves, num_leaves );
        evaluateCommandList( ctx, blend_branches, num_branches, blend_leaves, num_leaves );
    }

}}///
