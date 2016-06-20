#pragma once
#include "graph_public.h"
#include <util/ascii_script.h>

namespace bx
{
    void graphContextStartup();
    void graphContextShutdown();
    void graphContextCleanup( Scene* scene );
    void graphContextTick( Scene* scene );

    //////////////////////////////////////////////////////////////////////////
    void graphCreate( Graph** graph );
    void graphDestroy( Graph** graph, bool destroyNodes = true );
    void graphLoad( Graph* graph, const char* filename );
    bool graphNodeAdd( Graph* graph, id_t id );
    void graphNodeRemove( id_t id, bool destroyNode = false );
    void graphNodeLink( id_t parent, id_t child );
    void graphNodeUnlink( id_t child );

    //////////////////////////////////////////////////////////////////////////
    bool nodeRegister( const NodeTypeInfo* typeInfo );
    bool nodeCreate( id_t* out, const char* typeName, const char* nodeName );
    void nodeDestroy( id_t* inOut );


    //////////////////////////////////////////////////////////////////////////
    struct GraphSceneScriptCallback : public bxAsciiScript_Callback
    {
        virtual void addCallback( bxAsciiScript* script );
        virtual void onCreate( const char* typeName, const char* objectName );
        virtual void onAttribute( const char* attrName, const bxAsciiScript_AttribData& attribData );
        virtual void onCommand( const char* cmdName, const bxAsciiScript_AttribData& args );

        Graph* _graph = nullptr;
        id_t _current_id = makeInvalidHandle<id_t>();
    };

}///
