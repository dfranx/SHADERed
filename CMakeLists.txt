cmake_minimum_required(VERSION 2.8.9)
project(SHADERed)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

# our project info
include_directories(inc)
set(SOURCES
	main.cpp
	UI/UIHelper.cpp

# connectors
	EditorEngine.cpp
	GUIManager.cpp
	InterfaceManager.cpp

# objects:
	Objects/ArcBallCamera.cpp
	Objects/AudioAnalyzer.cpp
	Objects/DefaultState.cpp
	Objects/FirstPersonCamera.cpp
	Objects/FunctionVariableManager.cpp
	Objects/GizmoObject.cpp
	Objects/HLSL2GLSL.cpp
	Objects/KeyboardShortcuts.cpp
	Objects/Logger.cpp
	Objects/MessageStack.cpp
	Objects/Names.cpp
	Objects/ObjectManager.cpp
	Objects/PipelineManager.cpp
	Objects/ProjectParser.cpp
	Objects/RenderEngine.cpp
	Objects/Settings.cpp
	Objects/ShaderVariableContainer.cpp
	Objects/SystemVariableManager.cpp
	Objects/ThemeContainer.cpp

# UI
	UI/CodeEditorUI.cpp
	UI/CreateItemUI.cpp
	UI/MessageOutputUI.cpp
	UI/ObjectListUI.cpp
	UI/OptionsUI.cpp
	UI/PinnedUI.cpp
	UI/PipelineUI.cpp
	UI/PreviewUI.cpp
	UI/PropertyUI.cpp
	UI/VariableValueEdit.cpp

# engine:
	Engine/Timer.cpp
	Engine/Model.cpp
	Engine/GLUtils.cpp
	Engine/GeometryFactory.cpp
	Engine/Ray.cpp

# libraries:
	libs/ImGuiColorTextEdit/TextEditor.cpp
	libs/imgui/imgui_draw.cpp
	libs/imgui/imgui_widgets.cpp
	libs/imgui/imgui_demo.cpp
	libs/imgui/imgui.cpp
	libs/imgui/examples/imgui_impl_opengl3.cpp
	libs/imgui/examples/imgui_impl_sdl.cpp
	libs/SPIRVCross/spirv_cfg.cpp
	libs/SPIRVCross/spirv_cross.cpp
	libs/SPIRVCross/spirv_cross_util.cpp
	libs/SPIRVCross/spirv_cross_parsed_ir.cpp
	libs/SPIRVCross/spirv_glsl.cpp
	libs/SPIRVCross/spirv_parser.cpp
	libs/nativefiledialog/nfd_common.c
	libs/pugixml/src/pugixml.cpp
)


if(WIN32)
	set(SOURCES
		"${SOURCES}"
		libs/nativefiledialog/nfd_win.cpp
	)
else()
	set(SOURCES
		"${SOURCES}"
		libs/nativefiledialog/nfd_gtk.c
	)
	
	# also link gtk on linux
	FIND_PACKAGE(PkgConfig REQUIRED)
	PKG_CHECK_MODULES(GTK REQUIRED gtk+-3.0)
	INCLUDE_DIRECTORIES(${GTK_INCLUDE_DIRS})
	# Add other flags to the compiler
	LINK_DIRECTORIES(${GTK_LIBRARY_DIRS})
	ADD_DEFINITIONS(${GTK_CFLAGS})
endif()

# bruh
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "./cmake")

# sdl2
find_package(SDL2 REQUIRED)

# opengl
find_package(OpenGL REQUIRED)

# glew
find_package(GLEW REQUIRED)

# ASSIMP
find_package(ASSIMP REQUIRED)

# sfml audio
find_package(SFML COMPONENTS audio)

# library includes
include_directories(${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIR} ${SFML_INCLUDE_DIR})
include_directories(libs)

# glslang
add_subdirectory(libs/glslang)
include_directories(libs/glslang)

# create executable
add_executable(SHADERed ${SOURCES})

# link libraries
target_link_libraries(SHADERed ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${SDL2_MIXER_LIBRARY} ${ASSIMP_LIBRARIES} glslang SPIRV)
target_link_libraries(SHADERed sfml-audio)

if(WIN32)
	# link specific win32 libraries
	target_link_libraries(SHADERed Shell32.lib opengl32 SDL2::SDL2 SDL2::SDL2main)
	# TODO: try removing shell32.lib opengl32.lib
else()
	# link linux libraries
	# Link the target to the GTK+ libraries
	TARGET_LINK_LIBRARIES(SHADERed ${SDL2_LIBRARIES} ${GTK_LIBRARIES})
endif()

if (NOT MSVC)
	set_target_properties(SHADERed PROPERTIES CXX_STANDARD 17)
	target_compile_options(SHADERed PRIVATE -Wno-narrowing)
#	target_compile_options(SHADERed PRIVATE -fno-use-cxa-atexit)
endif()
