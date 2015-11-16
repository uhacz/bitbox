import maya.cmds as cmds
import sys
import os
import maya.OpenMaya as OpenMaya
import maya.OpenMayaMPx as OpenMayaMPx

def extractTransformFromPolyShape( polyShape ):
	shapeOutputAttr = "{0}.output".format( polyShape )
	shapeMesh = cmds.connectionInfo( shapeOutputAttr, dfs=1 )[0]
	shapeMesh = shapeMesh.split( "." )[0]
	shapeTransform = cmds.listRelatives( shapeMesh, parent=1 )
	return shapeTransform[0]

def exportDesignBlocks():
	spheres = cmds.ls( type="polySphere" )
	boxes = cmds.ls( type="polyCube" )
	sceneScript	= ""
	#print( spheres )
	#print( boxes )
	#print( cameras )
	for sph in spheres:
		transform = extractTransformFromPolyShape( sph )
		translate = cmds.getAttr( "{0}.translate".format(transform) )[0]
		rotate = cmds.getAttr( "{0}.rotate".format( transform ) )[0]

		sceneScript += "@dblock {0}\n".format( transform )
		sceneScript += "$shape {0}\n".format( cmds.getAttr( "{0}.radius".format( sph ) ) )
		sceneScript += "$pos {0} {1} {2}\n".format( translate[0], translate[1], translate[2] )
		sceneScript += "$rot {0} {1} {2}\n".format( rotate[0], rotate[1], rotate[2] )
		sceneScript += "$material \"grey\"\n"
		sceneScript += ":dblock_commit\n\n"
		
	for box in boxes:
		transform = extractTransformFromPolyShape( box )
		
		translate = cmds.getAttr( "{0}.translate".format(transform) )[0]
		rotate = cmds.getAttr( "{0}.rotate".format( transform ) )[0]
		shape = [ cmds.getAttr( "{0}.width".format( box ) ), cmds.getAttr( "{0}.height".format( box ) ), cmds.getAttr( "{0}.depth".format( box ) ) ]
		#print( shape )

		sceneScript += "@dblock {0}\n".format( transform )
		sceneScript += "$shape {0} {1} {2}\n".format( shape[0] * 0.5, shape[1] * 0.5, shape[2] * 0.5 )
		sceneScript += "$pos {0} {1} {2}\n".format( translate[0], translate[1], translate[2] )
		sceneScript += "$rot {0} {1} {2}\n".format( rotate[0], rotate[1], rotate[2] )
		sceneScript += "$material \"grey\"\n"
		sceneScript += ":dblock_commit\n\n"

	return sceneScript

def exportCameras():
	cameras = cmds.ls( cameras=1, leaf=1)
	#print( cameras )
	sceneScript = ""
	for camera in cameras:
		transform = cmds.listRelatives( camera, parent=1)[0]
		translate = cmds.getAttr( "{0}.translate".format(transform) )[0]
		rotate = cmds.getAttr( "{0}.rotate".format( transform ) )[0]

		sceneScript += "@camera {0}\n".format( transform )
		sceneScript += "$pos {0} {1} {2}\n".format( translate[0], translate[1], translate[2] )
		sceneScript += "$rot {0} {1} {2}\n".format( rotate[0], rotate[1], rotate[2] )
		sceneScript += "\n"


	return sceneScript


# command
class bxExportSceneCmd(OpenMayaMPx.MPxCommand):
	kPluginCmdName = "bxExportScene"

	def __init__(self):
		OpenMayaMPx.MPxCommand.__init__(self)

	@staticmethod
	def cmdCreator():
		return OpenMayaMPx.asMPxPtr( bxExportSceneCmd() )

	def doIt(self,argList):
		sceneScript = ""
		sceneScript += exportCameras()
		sceneScript += exportDesignBlocks()

		print( sceneScript )

		sceneName = cmds.file( q=1, sn=1, shn=1);
		filePath = os.environ['BX_ROOT'] + "assets/scene/" + sceneName.split( "." )[0] + ".scene"
		print (filePath )

		f = open( filePath, "w" )
		f.write( sceneScript )
		f.close()

# Initialize the script plug-in
def initializePlugin(plugin):
	pluginFn = OpenMayaMPx.MFnPlugin(plugin)
	try:
		pluginFn.registerCommand( bxExportSceneCmd.kPluginCmdName, bxExportSceneCmd.cmdCreator )
	except:
		sys.stderr.write("Failed to register command: %s\n" % bxExportSceneCmd.kPluginCmdName )
		raise

# Uninitialize the script plug-in
def uninitializePlugin(plugin):
	pluginFn = OpenMayaMPx.MFnPlugin(plugin)
	try:
		pluginFn.deregisterCommand(bxExportSceneCmd.kPluginCmdName)
	except:
		sys.stderr.write( "Failed to unregister command: %s\n" % bxExportSceneCmd.kPluginCmdName )
		raise