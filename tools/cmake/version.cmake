set(VERSION_INFO_MAJOR 1)
set(VERSION_INFO_MINOR 0)
set(VERSION_INFO_BUILD 0)

if(NOT
   DEFINED
   VERSION_INFO_EXTRA
)
  set(VERSION_INFO_EXTRA "git")
endif()

find_package(
  Git
  QUIET
)

if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --
    ERROR_QUIET
    RESULT_VARIABLE NOT_GIT_REPOSITORY
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  if(NOT_GIT_REPOSITORY)
    set(GIT_INFO "-unknown")
  else()
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD --
      OUTPUT_VARIABLE GIT_REV
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    execute_process(
      COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
      RESULT_VARIABLE GIT_DIRTY
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    if(GIT_DIRTY)
      set(GIT_INFO "-${GIT_REV}-dirty")
      set(GIT_IS_DIRTY true)
    else()
      set(GIT_INFO "-${GIT_REV}")
      set(GIT_IS_DIRTY false)
    endif()
  endif()

else()
  message(WARNING "git missing -- unable to check version.")
  unset(NOT_GIT_REPOSITORY)
  unset(GIT_REV)
  unset(GIT_DIRTY)
  unset(GIT_IS_DIRTY)
endif()

set(VERSION_INFO_BASE
    "v${VERSION_INFO_MAJOR}.${VERSION_INFO_MINOR}.${VERSION_INFO_BUILD}"
)

if("${VERSION_INFO_EXTRA}"
   STREQUAL
   "git"
)
  set(VERSION_INFO "${VERSION_INFO_BASE}-h${GIT_INFO}")
endif()

set(VERSION "${VERSION_INFO}")

configure_file(
  ${CMAKE_SOURCE_DIR}/src/common/app_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/app_build_version.h
  @ONLY
)
