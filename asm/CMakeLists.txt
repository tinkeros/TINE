if (WIN32)
  set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/win64.s)
else ()
  set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/sysv.s)
endif ()

add_library(FFI
  STATIC
  ${ASM_SRC}
)
target_compile_options(FFI
  PRIVATE
    -Wno-unused-command-line-argument
)
set_target_properties(FFI PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${PROJECT_NAME} PRIVATE FFI)

# vim: set expandtab ts=2 sw=2 :
