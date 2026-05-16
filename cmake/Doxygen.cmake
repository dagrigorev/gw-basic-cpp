option(GWBASIC_BUILD_DOCS "Build API documentation with Doxygen" OFF)

if(GWBASIC_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  set(GWBASIC_DOXYGEN_INPUT
      "${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/docs ${PROJECT_SOURCE_DIR}/README.md")
  set(GWBASIC_DOXYGEN_OUTPUT_DIR "${PROJECT_BINARY_DIR}/docs/api")
  configure_file(
      "${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
      "${PROJECT_BINARY_DIR}/Doxyfile"
      @ONLY)
  add_custom_target(docs
      COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Generating GW-BASIC API documentation"
      VERBATIM)
endif()
