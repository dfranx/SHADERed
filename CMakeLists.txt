cmake_minimum_required(VERSION 3.1)
project(SHADERed)

set(CMAKE_MODULE_PATH "./cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

# source code
set(SOURCES
	main.cpp
	UI/UIHelper.cpp

# connectors
	EditorEngine.cpp
	GUIManager.cpp
	InterfaceManager.cpp

# objects:
	Objects/PluginAPI/PluginManager.cpp
	Objects/ArcBallCamera.cpp
	Objects/AudioAnalyzer.cpp
	Objects/AudioShaderStream.cpp
	Objects/CameraSnapshots.cpp
	Objects/DefaultState.cpp
	Objects/FirstPersonCamera.cpp
	Objects/FunctionVariableManager.cpp
	Objects/GizmoObject.cpp
	Objects/ShaderTranscompiler.cpp
	Objects/KeyboardShortcuts.cpp
	Objects/Logger.cpp
	Objects/InputLayout.cpp
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
	Objects/UpdateChecker.cpp

# UI Tools
	UI/Tools/CubemapPreview.cpp
	UI/Tools/Magnifier.cpp

# UI
	UI/CodeEditorUI.cpp
	UI/CreateItemUI.cpp
	UI/MessageOutputUI.cpp
	UI/ObjectListUI.cpp
	UI/ObjectPreviewUI.cpp
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
		resource.rc
	)
elseif(APPLE)
	set(SOURCES
		"${SOURCES}"
		libs/nativefiledialog/nfd_cocoa.m
	)
else()
	set(SOURCES
		"${SOURCES}"
		libs/nativefiledialog/nfd_gtk.c
	)

	# link gtk on linux
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK REQUIRED gtk+-3.0)
	link_directories(${GTK_LIBRARY_DIRS})
	add_definitions(${GTK_CFLAGS})
endif()

# cmake toolchain
if(CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_TOOLCHAIN_FILE})
endif(CMAKE_TOOLCHAIN_FILE)

# sdl2
if(WIN32 OR APPLE OR NOT ${USE_PKG_CHECK_SDL})
  find_package(SDL2 REQUIRED)
else()
  # find sdl2 on linux
  pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# opengl
find_package(OpenGL REQUIRED)

# glew
find_package(GLEW REQUIRED)

# glm
find_package(GLM REQUIRED)

# ASSIMP
find_package(ASSIMP REQUIRED)

# sfml audio
find_package(SFML COMPONENTS system audio network)

# glslang
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_SPVREMAPPER OFF)
add_subdirectory(libs/glslang)

# create executable
add_executable(SHADERed ${SOURCES})

# properties
set_target_properties(SHADERed PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# include directories
target_include_directories(SHADERed PRIVATE ${SDL2_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIR} ${SFML_INCLUDE_DIR})
target_include_directories(SHADERed PRIVATE inc libs libs/glslang)
if (NOT WIN32)
	target_include_directories(SHADERed PRIVATE ${GTK_INCLUDE_DIRS})
endif()

# link libraries
target_link_libraries(SHADERed ${OPENGL_LIBRARIES} ${GLM_LIBRARY_DIRS} ${ASSIMP_LIBRARIES} glslang SPIRV)

# link sfml
if(${USE_FINDSFML})
	target_link_libraries(SHADERed ${SFML_LIBRARIES})
else()
	target_link_libraries(SHADERed sfml-audio sfml-system sfml-network)
endif()

if(WIN32)
	# link specific win32 libraries
	target_link_libraries(SHADERed GLEW::GLEW SDL2::SDL2 SDL2::SDL2main)

else()
	# link linux libraries
	target_link_libraries(SHADERed ${GLEW_LIBRARIES} ${SDL2_LIBRARIES} ${GTK_LIBRARIES} ${CMAKE_DL_LIBS})
endif()

if (NOT MSVC)
	target_compile_options(SHADERed PRIVATE -Wno-narrowing)
endif()

set(BINARY_INST_DESTINATION "bin")
set(RESOURCE_INST_DESTINATION "share/shadered")
install(PROGRAMS bin/SHADERed DESTINATION "${BINARY_INST_DESTINATION}" RENAME shadered)
install(DIRECTORY bin/data bin/templates bin/themes bin/plugins DESTINATION "${RESOURCE_INST_DESTINATION}")

if (UNIX AND NOT APPLE)
	# install XDG desktop file and icon on Linux/BSD
	install(FILES Misc/Linux/io.github.dfranx.SHADERed.desktop DESTINATION "share/applications" RENAME shadered.desktop)
	install(FILES bin/icon.png DESTINATION "share/icons/hicolor/64x64/apps" RENAME shadered.png)
endif()
