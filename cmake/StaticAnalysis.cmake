option(GWBASIC_ENABLE_CLANG_TIDY "Run clang-tidy during the build" OFF)
option(GWBASIC_ENABLE_CPPCHECK "Run Cppcheck during the build" OFF)

if(GWBASIC_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR "GWBASIC_ENABLE_CLANG_TIDY is ON, but clang-tidy was not found")
  endif()
  set(CMAKE_CXX_CLANG_TIDY
      "${CLANG_TIDY_EXE};--warnings-as-errors=*"
      CACHE STRING "clang-tidy command" FORCE)
endif()

if(GWBASIC_ENABLE_CPPCHECK)
  find_program(CPPCHECK_EXE NAMES cppcheck)
  if(NOT CPPCHECK_EXE)
    message(FATAL_ERROR "GWBASIC_ENABLE_CPPCHECK is ON, but cppcheck was not found")
  endif()
  set(CMAKE_CXX_CPPCHECK
      "${CPPCHECK_EXE};--enable=warning,style,performance,portability;--inline-suppr;--error-exitcode=1"
      CACHE STRING "cppcheck command" FORCE)
endif()
