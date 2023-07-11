# If dreams can come true what does that say about nightmares?
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

# thank you justin
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  message(FATAL_ERROR "You cannot use CMake to build from the root of its source tree! Remove the CMakeCache.txt file and CMakeFiles directory from this directory, then create a separate directory (either below this directory or elsewhere), and then re-run CMake from there.")
endif ()

# lmao lol
set(PROJECT_MAJOR_VERSION 4)
set(PROJECT_MINOR_VERSION 2)
set(PROJECT_PATCH_LEVEL   0)
set(VERSION_SIMPLE "${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_LEVEL}")

project(tine
  VERSION ${VERSION_SIMPLE}
  DESCRIPTION "TINE Is Not an Emulator"
  LANGUAGES C CXX
)

# set a default build type if none was specified
set(default_build_type "Release")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}"
    CACHE
      STRING "Choose the type of build." FORCE
  )
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE
    PROPERTY
      STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
  )
endif ()
message(STATUS "${PROJECT_NAME} version: ${VERSION_SIMPLE}")
option(WARNINGS_AS_ERRORS "Set certain warnings to be treated as errors by the compiler" ON)


set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include (DetectArch)
target_architecture(ARCH)
if (NOT ARCH MATCHES "x86_64")
  message(FATAL_ERROR "CAN ONLY BUILD ON x86_64 ARCHITECTURE")
endif ()

set(CXX_EXECUTABLE_SOURCE_FILES
  main.cxx
  dbg.cxx
  sdl_window.cxx
  runtime.cxx
  tos_aot.cxx
  mem.cxx
  multic.cxx
  vfs.cxx
  TOSPrint.cxx
  ext/linenoise/linenoise.cpp
  ext/linenoise/ConvertUTF.cpp
  ext/linenoise/wcwidth.cpp
)

set(C_EXECUTABLE_SOURCE_FILES
  sound.c
  ext/dyad.c
  ext/argtable3.c
)

set_source_files_properties(CXX_EXECUTABLE_SOURCE_FILES
  PROPERTIES
    LANGUAGE CXX
    COMPILE_FLAGS -fno-rtti
)

set_source_files_properties(C_EXECUTABLE_SOURCE_FILES}
  PROPERTIES
    LANGUAGE C
    COMPILE_FLAGS -Werror=implicit-function-declaration
)

add_executable(${PROJECT_NAME}
  ${C_EXECUTABLE_SOURCE_FILES}
  ${CXX_EXECUTABLE_SOURCE_FILES}
)

# outputs bin to src tree dir
set_target_properties(${PROJECT_NAME}
  PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
    C_EXTENSIONS YES
    LINKER_LANGUAGE CXX
    INTERPROCEDURAL_OPTIMIZATION ON
)

target_compile_features(${PROJECT_NAME}
  PRIVATE
    cxx_thread_local
    cxx_right_angle_brackets
    cxx_range_for
    cxx_auto_type
    cxx_nullptr
    cxx_alignas
    cxx_constexpr
    cxx_variadic_macros
    cxx_uniform_initialization
    cxx_strong_enums
    cxx_static_assert
    cxx_lambdas
    cxx_generic_lambdas
    cxx_generalized_initializers
    cxx_attributes
    cxx_aggregate_default_initializers
)

target_compile_options(${PROJECT_NAME}
  PRIVATE
    -Werror=shadow
    -fno-omit-frame-pointer
    -m64
    -fno-exceptions
    -Wall
    -Wextra
    -Wno-frame-address
    -fvisibility=hidden
)

# libm
target_link_libraries(${PROJECT_NAME} PRIVATE m)

# sdl2 dep
find_package(SDL2 REQUIRED)
if (NOT SDL2_FOUND)
  message(FATAL_ERROR "INSTALL SDL2")
endif ()
target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)

if (WIN32)
  # I literally gave up trying to build this shit statically
  # so bundle your own dlls when you want to package it
  target_link_libraries(${PROJECT_NAME} PRIVATE
    ws2_32 synchronization shlwapi shcore dbghelp winmm -static)
  set(C_ABI_NAME "WIN64")
  set(BIN_FORMAT "win64")
  unset(CMAKE_THREAD_PREFER_PTHREAD)
  unset(THREADS_PREFER_PTHREAD_FLAG)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
        CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  # ffi
  set(C_ABI_NAME "SYSV")
  set(BIN_FORMAT "elf64")
  # uses this because cmake cant detect builtins
  include (CheckCXXSourceCompiles)
  check_cxx_source_compiles(
    "#include <stdint.h>
     int main() {
       uint32_t i = 1, j = 1;
       __atomic_compare_exchange_n(
         &i, &j, 0, false,
         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST
      );
      return !(i == 0 && j == 1);
    }" HAS_ATOMIC_LOAD_N)
  if (NOT HAS_ATOMIC_LOAD_N)
    message(FATAL_ERROR "No builtin atomics, wtf are you using as your compiler?")
  endif ()
else ()
  message(FATAL_ERROR "Unsupported system")
endif ()

# ffi
set(FFI_NAME "FFI_${C_ABI_NAME}")
add_custom_command(OUTPUT "${FFI_NAME}.obj"
  COMMAND yasm
  ARGS -f ${BIN_FORMAT} -o "${CMAKE_CURRENT_BINARY_DIR}/${FFI_NAME}.obj" "${CMAKE_CURRENT_SOURCE_DIR}/${FFI_NAME}.yasm"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FFI_NAME}.yasm"
)
add_library(FFI STATIC "${FFI_NAME}.obj")
set_target_properties(FFI
  PROPERTIES
    LINKER_LANGUAGE C
)
target_link_libraries(${PROJECT_NAME} PRIVATE FFI)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
