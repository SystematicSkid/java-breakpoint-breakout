cmake_minimum_required( VERSION 3.20 FATAL_ERROR )
project( java-intercepter )

set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Enable MASM
enable_language( ASM_MASM )

# Ensure MASM compiler is available
if( NOT CMAKE_ASM_MASM_COMPILER )
    message( FATAL_ERROR "MASM compiler not found" )
endif()

# Recursive find all source files
file( GLOB_RECURSE SOURCES "src/*.cpp" "src/*.asm" )
# Includes ./include
include_directories( include )

# Find JNI library
find_package( JNI REQUIRED )

# Include jni headers
include_directories( ${JNI_INCLUDE_DIRS} )

# Create a shared library from sources
add_library( ${PROJECT_NAME} SHARED ${SOURCES} )

# Link jvm
target_link_libraries( ${PROJECT_NAME} ${JNI_LIBRARIES} )