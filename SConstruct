#
# Top-level SConstruct file for Miracle Grue.
#

# env = Environment(tools=['default','qt4']

# On Mac builds, scons complains that Qt4 can't be found. By mergeing the PATH environment variable
# the moc tool is detected and Qt4 is detected 
import os
import commands
import datetime


def mix(*args):
	l = []
	for arg_list in args:
		l += arg_list
	
	no_duplicates =  list(set(l)) # remove duplicates
	#print "***", no_duplicates
	scons_list = [env.Object(x) for x in no_duplicates]
	return scons_list

print ""
print "======================================================="
print "Miracle-Grue build script"
print " * it is now", datetime.datetime.now(), " (Qt and cppUnit are sold separately)"



# Using just one environemt setup for now	
env = Environment(ENV = {'PATH' : os.environ['PATH']}, tools=['default','qt4'])
# print "os.environ['PATH']=", os.environ['PATH']

debug = ARGUMENTS.get('debug', 0)
if int(debug):
    env.Append(CCFLAGS = '-g')
       
qtModules = ['QtCore', 'QtNetwork' ]
print "QT modules", qtModules
env.EnableQt4Modules(qtModules)


env.Library('./bin/lib/bgl', ['src/BGL/BGLAffine.cc',
    'src/BGL/BGLBounds.cc',
    'src/BGL/BGLCommon.cc',
    'src/BGL/BGLCompoundRegion.cc',
    'src/BGL/BGLIntersection.cc',
    'src/BGL/BGLLine.cc',
    'src/BGL/BGLMesh3d.cc',
    'src/BGL/BGLPath.cc',
    'src/BGL/BGLPoint.cc',
    'src/BGL/BGLPoint3d.cc',
    'src/BGL/BGLSimpleRegion.cc',
    'src/BGL/BGLTriangle3d.cc',
    'src/BGL/BGLSVG.cc'])



env.Library('./bin/lib/_json', ['src/json-cpp/src/lib_json/json_reader.cpp',
                       'src/json-cpp/src/lib_json/json_value.cpp',
                       'src/json-cpp/src/lib_json/json_writer.cpp' ],
            CPPPATH=['src/json-cpp/include'])

env.Library('./bin/lib/miracleGrue',['src/Operation.cc'], CPPATH=['src/'])

unit_test = ['src/unit_tests/UnitTestMain.cc',]
config 	  = ['src/Configuration.cc']
file_w    = ['src/Configuration.cc', 'src/FileWriterOperation.cc', 'src/GCodeEnvelope.cc',]
gcoder    = ['src/Configuration.cc', 'src/GCoderOperation.cc', 'src/PathData.cc', 'src/GCodeEnvelope.cc',]
pather    = ['src/Configuration.cc', 'src/PatherOperation.cc', 'src/PathData.cc', 'src/RegionData.cc',]
regioner  = ['src/Configuration.cc', 'src/RegionerOperation.cc','src/RegionData.cc','src/SliceData.cc',]
slicer    = ['src/Configuration.cc', 'src/SliceOperation.cc', 'src/MeshData.cc', 'src/RegionData.cc',]
file_r    = ['src/Configuration.cc', 'src/ModelFileReaderOperation.cc', 'src/MeshData.cc',]
example_op   = ['src/Configuration.cc', 'src/ExampleOperation.cc',]

default_includes = ['..','src/json-cpp/include', 'src', 'src/BGL']
default_libs = [ '_json','miracleGrue', 'bgl']
default_libs_path = ['/usr/lib', '/usr/local/lib', './bin/lib']

debug_libs = ['cppunit',]
debug_libs_path = ["", ]

mand_ops = ['src/Configuration.cc', 
	'src/MandTest/MandStlLoaderOperation.cc','src/MandTest/StlEnvelope.cc' ,
	'src/MandTest/MandCarveOperation.cc','src/MandTest/RegionEnvelope.cc',
	'src/MandTest/MandInsetOperation.cc', 'src/MandTest/ShellEnvelope.cc',
	'src/MandTest/MandInfillOperation.cc',
	'src/MandTest/MandWriteSvgOperation.cc']

env.Program(	'./farMandolineTest',
				mix(['FarScratchpad.cc'],
					mand_ops ),
				LIBS = default_libs + ['bgl'],
				LIBPATH = default_libs_path, 
				CPPPATH = default_includes )

env.Program(	'./bin/tests/exampleOpUnitTest',
				mix(['src/unit_tests/ExampleOpTestCase.cc'],
					file_w, config, unit_test, example_op),
				LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path + debug_libs_path, 
				CPPPATH = default_includes)


env.Program(	'./bin/tests/fileWriterUnitTest',
				mix(['src/unit_tests/FileWriterTestCase.cc'],
					file_w, config, unit_test),
				LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path + debug_libs_path, 
				CPPPATH = default_includes)

#env.Program(	'./bin/tests/queryInterfaceUnitTest',
#				mix(['src/unit_tests/QueryInterfaceTestCase.cc'],
#					file_w, config, unit_test),
#				LIBS = default_libs + debug_libs,
#				LIBPATH = default_libs_path, 
#				CPPPATH = default_includes)
				
env.Program(	'./bin/tests/configUnitTest',
				mix(['src/unit_tests/ConfigTestCase.cc'],config, unit_test),
				LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path , 
				CPPPATH = default_includes)

env.Program( 	'./bin/tests/gcoderUnitTest', 
				mix(['src/unit_tests/GCoderTestCase.cc'], 
				unit_test, pather, gcoder, file_w), 
				LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path + debug_libs_path, 
				CPPPATH= default_includes)

env.Program( 	'./bin/tests/slicerUnitTest', 
				mix(['src/unit_tests/SlicerTestCase.cc'], unit_test, slicer), 
				LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path + debug_libs_path, 
				CPPPATH= ['..'])

env.Program(  	'./bin/tests/regionerUnitTest',   
				mix(['src/unit_tests/RegionerTestCase.cc'], unit_test, regioner), 
				LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path + debug_libs_path, 
				CPPPATH= ['..'])

env.Program(  	'./bin/tests/modelReaderUnitTest',   
				mix(['src/unit_tests/ModelReaderTestCase.cc'], unit_test, file_r), 
				LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path + debug_libs_path, 
				CPPPATH= ['..'])

env.Program(  	'./bin/tests/chainIntegrationUnitTest',   
				mix(['src/unit_tests/ChainIntegrationTestCase.cc'], unit_test, file_r, slicer, regioner, pather, gcoder, file_w), 
    			LIBS = default_libs + debug_libs,
				LIBPATH = default_libs_path + debug_libs_path, 
				CPPPATH= ['..'])
