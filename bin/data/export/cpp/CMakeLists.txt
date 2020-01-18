cmake_minimum_required(VERSION 3.1)
project([$$project_name$$])

set(CMAKE_MODULE_PATH "./cmake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ./bin)

# source code
set(SOURCES
	[$$project_file$$]
)

# cmake toolchain 
if(CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_TOOLCHAIN_FILE})
endif(CMAKE_TOOLCHAIN_FILE)

# sdl2
if(WIN32 OR APPLE OR NOT ${USE_PKG_CHECK_SDL})
  find_package(SDL2 REQUIRED)
else()
  # find sdl2 on linux
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# opengl
find_package(OpenGL REQUIRED)

# glew
find_package(GLEW REQUIRED)

# glm
find_package(GLM REQUIRED)

# create executable
add_executable([$$project_name$$] ${SOURCES})

# properties
set_target_properties([$$project_name$$] PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# include directories
target_include_directories([$$project_name$$] PRIVATE  ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})

# link libraries
target_link_libraries([$$project_name$$] ${GLM_LIBRARY_DIRS} ${OPENGL_LIBRARIES})

if(WIN32)
	# link specific win32 libraries
	target_link_libraries([$$project_name$$] GLEW::GLEW SDL2::SDL2)
else()
	# link linux libraries
	target_link_libraries([$$project_name$$] ${GLEW_LIBRARIES} ${SDL2_LIBRARIES})
endif()

if (NOT MSVC)
	target_compile_options([$$project_name$$] PRIVATE -Wno-narrowing)
endif()
