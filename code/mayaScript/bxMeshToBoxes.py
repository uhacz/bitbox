import maya.api.OpenMaya as om
import maya.cmds as cmds 

def maya_useNewAPI():
    pass
    
meshDagPath = om.MGlobal.getActiveSelectionList().getDagPath(0)
print( meshDagPath )

meshFn = om.MFnMesh( meshDagPath )

pIt = om.MItMeshPolygon( meshDagPath )
while not pIt.isDone():
    area = pIt.getArea()
    center = pIt.center()
    normal = pIt.getNormal()
    tangentIndex = pIt.vertexIndex(0)
    
    tangent = meshFn.getFaceVertexTangent( pIt.index(), tangentIndex )
    binormal = meshFn.getFaceVertexBinormal( pIt.index(), tangentIndex )
        
    #print( center )
    #print( normal )
    #print( tangent )
    #print( binormal )
    
    matrix = om.MMatrix()
    matrix[0] = binormal.x
    matrix[1] = binormal.y
    matrix[2] = binormal.z
    matrix[3] = 0
    
    matrix[4] = normal.x
    matrix[5] = normal.y
    matrix[6] = normal.z
    matrix[7] = 0
    
    matrix[8] = tangent.x
    matrix[9] = tangent.y
    matrix[10] = tangent.z
    matrix[11] = 0
    
    matrix[12] = center.x
    matrix[13] = center.y
    matrix[14] = center.z
    eulerRot = om.MTransformationMatrix( matrix ).rotation()
    
    #print( eulerRot )
    #print( "\n" )
    nodes = cmds.polyCube( w=area, h=area, d=area )
    #print( nodes )
        
    sList = om.MSelectionList()
    sList.add( nodes[0] )
    
    tr = om.MFnTransform( sList.getDagPath(0) )
    #print( om.MVector( center ) )
    tr.setTranslation( om.MVector( center ), om.MSpace.kWorld )
    tr.setRotation( eulerRot, om.MSpace.kTransform )
    pIt.next( 0 )