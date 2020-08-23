cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 17)
project(SHADERed)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

option(BUILD_IMMEDIATE_MODE "Build the immediate mode related features" OFF)

# source code
set(SOURCES
	main.cpp
	src/SHADERed/UI/UIHelper.cpp

# connectors
	src/SHADERed/EditorEngine.cpp
	src/SHADERed/GUIManager.cpp
	src/SHADERed/InterfaceManager.cpp

# objects:
	src/SHADERed/Objects/Export/ExportCPP.cpp
	src/SHADERed/Objects/ArcBallCamera.cpp
	src/SHADERed/Objects/AudioAnalyzer.cpp
	src/SHADERed/Objects/AudioShaderStream.cpp
	src/SHADERed/Objects/CameraSnapshots.cpp
	src/SHADERed/Objects/CommandLineOptionParser.cpp
	src/SHADERed/Objects/DefaultState.cpp
	src/SHADERed/Objects/DebugInformation.cpp
	src/SHADERed/Objects/FirstPersonCamera.cpp
	src/SHADERed/Objects/FunctionVariableManager.cpp
	src/SHADERed/Objects/GizmoObject.cpp
	src/SHADERed/Objects/ShaderCompiler.cpp
	src/SHADERed/Objects/KeyboardShortcuts.cpp
	src/SHADERed/Objects/Logger.cpp
	src/SHADERed/Objects/InputLayout.cpp
	src/SHADERed/Objects/MessageStack.cpp
	src/SHADERed/Objects/Names.cpp
	src/SHADERed/Objects/ObjectManager.cpp
	src/SHADERed/Objects/PipelineManager.cpp
	src/SHADERed/Objects/ProjectParser.cpp
	src/SHADERed/Objects/RenderEngine.cpp
	src/SHADERed/Objects/Settings.cpp
	src/SHADERed/Objects/ShaderVariableContainer.cpp
	src/SHADERed/Objects/SPIRVParser.cpp
	src/SHADERed/Objects/SystemVariableManager.cpp
	src/SHADERed/Objects/ThemeContainer.cpp
	src/SHADERed/Objects/PluginManager.cpp
	src/SHADERed/Objects/WebAPI.cpp

# UI Tools
	src/SHADERed/UI/Tools/CubemapPreview.cpp
	src/SHADERed/UI/Tools/Magnifier.cpp
	src/SHADERed/UI/Tools/StatsPage.cpp
	src/SHADERed/UI/Tools/VariableValueEdit.cpp
	src/SHADERed/UI/Tools/DebuggerOutline.cpp
	src/SHADERed/UI/Tools/NotificationSystem.cpp

# UI Debug
	src/SHADERed/UI/Debug/BreakpointListUI.cpp
	src/SHADERed/UI/Debug/FunctionStackUI.cpp
	src/SHADERed/UI/Debug/ImmediateUI.cpp
	src/SHADERed/UI/Debug/ValuesUI.cpp
	src/SHADERed/UI/Debug/WatchUI.cpp

# UI
	src/SHADERed/UI/CodeEditorUI.cpp
	src/SHADERed/UI/CreateItemUI.cpp
	src/SHADERed/UI/MessageOutputUI.cpp
	src/SHADERed/UI/ObjectListUI.cpp
	src/SHADERed/UI/ObjectPreviewUI.cpp
	src/SHADERed/UI/OptionsUI.cpp
	src/SHADERed/UI/PinnedUI.cpp
	src/SHADERed/UI/PipelineUI.cpp
	src/SHADERed/UI/PixelInspectUI.cpp
	src/SHADERed/UI/PreviewUI.cpp
	src/SHADERed/UI/PropertyUI.cpp

# engine:
	src/SHADERed/Engine/Timer.cpp
	src/SHADERed/Engine/Model.cpp
	src/SHADERed/Engine/GLUtils.cpp
	src/SHADERed/Engine/GeometryFactory.cpp
	src/SHADERed/Engine/Ray.cpp

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
	libs/pugixml/src/pugixml.cpp
	libs/ShaderExpressionParser/Parser.cpp
	libs/ShaderExpressionParser/Tokenizer.cpp
	libs/ImGuiFileDialog/ImGuiFileDialog.cpp
)


if(WIN32)
	set(SOURCES
		"${SOURCES}"
		resource.rc
	)
endif()

# cmake toolchain
if(CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_TOOLCHAIN_FILE})
endif(CMAKE_TOOLCHAIN_FILE)

# pkg
if (UNIX)
  find_package(PkgConfig REQUIRED)
endif()

# sdl2
if(WIN32 OR APPLE OR NOT ${USE_PKG_CHECK_SDL})
  find_package(SDL2 REQUIRED)
else()
  pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# opengl
find_package(OpenGL REQUIRED)

# glew
find_package(GLEW REQUIRED)

# glm
find_package(GLM REQUIRED)

# sfml audio
find_package(SFML COMPONENTS system audio network)

# SpvGenTwo
if(BUILD_IMMEDIATE_MODE)
	set(SOURCES
		"${SOURCES}"
		src/SHADERed/Objects/Debug/ExpressionCompiler.cpp
	)
	add_subdirectory(libs/SpvGenTwo)
endif()

# assimp
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static lib" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Assimp's tools" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(ASSIMP_WERROR OFF CACHE BOOL "Warnings as errors" FORCE)
add_subdirectory(libs/assimp)

# spirv-headers
set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "Skip example" FORCE)
add_subdirectory(libs/spirv-headers)

# spirv-tools
set(SPIRV_BUILD_FUZZER OFF CACHE BOOL "Build the spirv-fuzz tool" FORCE)
set(SPIRV_COLOR_TERMINAL OFF CACHE BOOL "Enables color console output" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "Build only the library and the command line tools" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Build only the library" FORCE)
set(SPIRV_WERROR OFF CACHE BOOL "Forces a compilation error on any warnings encountered" FORCE)
add_subdirectory(libs/spirv-tools)

# glslang
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Enables building of SPVRemapper" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Builds glslangValidator and spirv-remap" FORCE)
add_subdirectory(libs/glslang)

# SPIRV-VM
add_subdirectory(libs/SPIRV-VM)

# Native macOS frameworks
if (APPLE)
	# Link Homebrew library path for Mojave and up
	# See: https://stackoverflow.com/questions/54068035/linking-not-working-in-homebrews-cmake-since-mojave
	link_directories(/usr/local/lib)
	set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework AppKit -framework AudioToolbox -framework AudioUnit -framework Carbon -framework Cocoa -framework CoreAudio -framework CoreVideo -framework ForceFeedback -framework IOKit -framework OpenGL -framework CoreServices -framework Security")
	set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -framework AppKit -framework AudioToolbox -framework AudioUnit -framework Carbon -framework Cocoa -framework CoreAudio -framework CoreVideo -framework ForceFeedback -framework IOKit -framework OpenGL -framework CoreServices -framework Security")
endif()

# create executable
add_executable(SHADERed ${SOURCES})

# properties
set_target_properties(SHADERed PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# preprocessor
if (BUILD_IMMEDIATE_MODE)
	target_compile_definitions(SHADERed PRIVATE BUILD_IMMEDIATE_MODE)
endif()

# include directories
target_include_directories(SHADERed PRIVATE ${SDL2_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIR} ${SFML_INCLUDE_DIR})
target_include_directories(SHADERed PRIVATE src libs libs/glslang libs/SPIRV-VM/inc)
if (NOT WIN32)
	target_include_directories(SHADERed PRIVATE ${GTK_INCLUDE_DIRS})
endif()

# link libraries 
target_link_libraries(SHADERed ${OPENGL_LIBRARIES} ${GLM_LIBRARY_DIRS} glslang SPIRV SPIRVVM assimp::assimp)

# link SpvGenTwo
if (BUILD_IMMEDIATE_MODE)
	target_link_libraries(SHADERed SpvGenTwoLib SpvGenTwoCommon)
endif()

# link sfml
if(${USE_FINDSFML})
	target_link_libraries(SHADERed ${SFML_LIBRARIES})
else()
	target_link_libraries(SHADERed sfml-audio sfml-system sfml-network)
endif()

if(WIN32)
	# link specific win32 libraries
	target_link_libraries(SHADERed GLEW::GLEW SDL2::SDL2 SDL2::SDL2main)

elseif(UNIX AND NOT APPLE)
	# link linux libraries
	target_link_libraries(SHADERed ${GLEW_LIBRARIES} ${SDL2_LIBRARIES} ${GTK_LIBRARIES} ${CMAKE_DL_LIBS} stdc++fs)
elseif(APPLE)
	target_link_libraries(SHADERed GLEW::GLEW ${SDL2_LIBRARIES} ${GTK_LIBRARIES} ${CMAKE_DL_LIBS})
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
	install(FILES Misc/Linux/org.shadered.SHADERed.desktop DESTINATION "share/applications" RENAME shadered.desktop)
	install(FILES bin/icon_32x32.png DESTINATION "share/icons/hicolor/32x32/apps" RENAME shadered.png)
	install(FILES bin/icon_64x64.png DESTINATION "share/icons/hicolor/64x64/apps" RENAME shadered.png)
	install(FILES bin/icon_128x128.png DESTINATION "share/icons/hicolor/128x128/apps" RENAME shadered.png)
	install(FILES bin/icon_256x256.png DESTINATION "share/icons/hicolor/256x256/apps" RENAME shadered.png)
endif()
