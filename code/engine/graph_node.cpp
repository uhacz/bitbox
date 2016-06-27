#include "graph_node.h"
#include "graph_context.h"
#include "graph.h"

namespace bx
{
    SceneGraph* NodeInstanceInfo::sceneGraph() const
    {
        return &_graph->_scene_graph;
    }

    SceneGraph* NodeInstanceInfo::sceneGraph()
    {
        return &_graph->_scene_graph;
    }

    TransformInstance NodeUtil::addToSceneGraph( const NodeInstanceInfo& instance, const Matrix4& pose )
    {
        return instance.sceneGraph()->create( instance._id, pose );
    }

    void NodeUtil::removeFromSceneGraph( const NodeInstanceInfo& instance )
    {
        SceneGraph* sg = instance.sceneGraph();
        TransformInstance ti = sg->get( instance._id );
        if( sg->isValid( ti ) )
        {
            sg->destroy( ti );
        }
    }

}///