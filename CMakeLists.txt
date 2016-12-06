cmake_minimum_required(VERSION 2.8)

project(epilog)

set(epilog_CXX_SRCS
# Compile the Pegmatite .cc files directly into the program (rather than creating a separate library).
lib/Pegmatite/ast.cc
lib/Pegmatite/parser.cc
src/compiler.cc
src/interpreter.cc
src/main.cc
src/runtime.cc
)
set(LLVM_LIBS
all
)

# Define the Epilog program that we will build.
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
add_executable(epilog ${epilog_CXX_SRCS})
# We're using Pegmatite in the RTTI mode.
add_definitions(-DUSE_RTTI=1)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
set(CLANG_FLAGS "-Wdeprecated -Wno-zero-length-array -Wno-unknown-attributes")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_FLAGS}")
else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()


find_program(LLVM_CONFIG NAMES llvm-config DOC "Path to llvm-config utility")
if (${LLVM_CONFIG} STREQUAL "LLVM_CONFIG-NOTFOUND")
message(SEND_ERROR "llvm-config not found, please manually set path with -DLLVM_CONFIG")
endif()
message(STATUS "Using llvm-config: ${LLVM_CONFIG}")

# Define LLVM version macros so that we can support multiple versions in the source.
exec_program(${LLVM_CONFIG}
ARGS --version
OUTPUT_VARIABLE LLVM_VER)
exec_program(${LLVM_CONFIG}
ARGS --cxxflags
OUTPUT_VARIABLE LLVM_CXXFLAGS)
exec_program(${LLVM_CONFIG}
ARGS --libs ${LLVM_LIBS}
OUTPUT_VARIABLE LLVM_LIBS_FLAGS)
exec_program(${LLVM_CONFIG}
ARGS --ldflags
OUTPUT_VARIABLE LLVM_LDFLAGS)
exec_program(${LLVM_CONFIG}
ARGS --system-libs
OUTPUT_VARIABLE LLVM_SYSTEMLIBS)
exec_program(${LLVM_CONFIG}
ARGS --libdir
OUTPUT_VARIABLE LLVM_LIBDIR)
exec_program(${LLVM_CONFIG}
ARGS --bindir
OUTPUT_VARIABLE LLVM_BINDIR)
string(REGEX REPLACE "([0-9]*).([0-9]*).*" "-DLLVM_MAJOR=\\1 -DLLVM_MINOR=\\2" LLVM_VERSION "${LLVM_VER}")


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXXFLAGS} ${LLVM_VERSION}")
target_link_libraries(epilog ${LLVM_LIBS_FLAGS})
# llvm-config only gained a --system-libs flag in 3.5
if (LLVM_VER VERSION_GREATER 3.4)
target_link_libraries(epilog ${LLVM_SYSTEMLIBS})
endif()
set(CMAKE_EXE_LINKER_FLAGS "${LLVM_LDFLAGS} ${LIBGC} ${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,${LLVM_LIBDIR}")

# Make sure that we use the LLVM path as an rpath so that we can dynamically
# link to LLVM.  Don't let CMake specify its own rpath.
set(CMAKE_SKIP_RPATH true)

# Make sure that LLVM is able to find functions in the main executable.
SET_TARGET_PROPERTIES(epilog PROPERTIES
ENABLE_EXPORTS TRUE)