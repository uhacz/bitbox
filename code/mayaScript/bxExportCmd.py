import maya.cmds as cmds
spheres = cmds.ls( type="polySphere" )
boxes = cmds.ls( type="polyCube" )
cameras = cmds.ls( cameras=1, leaf=1)

print( spheres )
print( boxes )
print( cameras )

sph = spheres[0]
sphereOutputAttr = "{0}.output".format( sph )
sphereMesh = cmds.connectionInfo( sphereOutputAttr, dfs=1 )[0]
sphereMesh = sphereMesh.split( "." )[0]
sphereTransform = cmds.listRelatives( sphereMesh, parent=1 )
print( sphereTransform )

bxScriptCmd = "@dblock {0}\n".format( sphereTransform )
bxScriptCmd += "$shape {0}\n".format( cmds.getAttr( "{0}.radius".format( sph ) ) )
bxScriptCmd += "$pos {0} {1} {2}".format( cmds.getAttr( "{0}.translate".format(sphereTransform) ) )
print( bxScriptCmd )
