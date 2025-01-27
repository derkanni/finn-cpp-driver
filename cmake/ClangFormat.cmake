find_program(CLANG_FORMAT_FOUND "clang-format")
if (NOT CLANG_FORMAT_FOUND)
    message(WARNING "clang-format requested, but not found!")
endif()

#find all files to check
file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.h ${PROJECT_SOURCE_DIR}/src/*.hpp)

add_custom_target(
  check_format
  COMMAND clang-format
  -Werror
  --dry-run
  -style=file
  ${ALL_SOURCE_FILES}
)

add_custom_target(
  format
  COMMAND clang-format
  -style=file
  -i
  ${ALL_SOURCE_FILES}
)
