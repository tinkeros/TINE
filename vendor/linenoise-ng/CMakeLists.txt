file(GLOB LINENOISE_NG_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(linenoise-ng
  STATIC
  ${LINENOISE_NG_SRC}
)

target_compile_options(linenoise-ng
  PRIVATE
    -m64
    -Wall
    -Wextra
    -Wpedantic
    -Werror=shadow
    -fno-exceptions
    -fno-rtti
    -mno-shstk
    -fno-asynchronous-unwind-tables
    -fno-stack-protector
    -fno-unwind-tables
    -fcf-protection=none
)

set_target_properties(linenoise-ng
  PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    CXX_VISIBILITY_PRESET hidden
    INTERPROCEDURAL_OPTIMIZATION ON
    LINKER_LANGUAGE CXX
)

target_link_libraries(${PROJECT_NAME} PRIVATE linenoise-ng)

# vim: set expandtab ts=2 sw=2 :
