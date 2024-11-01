cmake_minimum_required(VERSION 3.10)
project(libarchbloom VERSION 1.0 LANGUAGES C)

include(GNUInstallDirs)

# Set the output directory for libraries and executables
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add source files for the library
set(SRC_FILES
    src/mmh3.c
    src/bloom.c
    src/cbloom.c
    src/tdbloom.c
    src/tdcbloom.c
    src/cuckoo.c
    src/gaussiannb.c
)

# Create a static library (.a)
add_library(archbloom_static STATIC ${SRC_FILES})
set_target_properties(archbloom_static PROPERTIES OUTPUT_NAME "archbloom-static")

# Create a shared library (.so)
add_library(archbloom_shared SHARED ${SRC_FILES})
set_target_properties(archbloom_shared PROPERTIES OUTPUT_NAME "archbloom")

# Include directories for the library headers
target_include_directories(archbloom_static PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(archbloom_shared PUBLIC ${PROJECT_SOURCE_DIR}/src)

# Link against the math library for both shared and static versions
target_link_libraries(archbloom_static PUBLIC m)
target_link_libraries(archbloom_shared PUBLIC m)

# Test programs
set(TEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tests)

add_executable(test_bloom_basic tests/test_bloom_basic.c)
set_target_properties(test_bloom_basic PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
target_link_libraries(test_bloom_basic PRIVATE archbloom_shared)

add_executable(test_cbloom_basic tests/test_cbloom_basic.c)
set_target_properties(test_cbloom_basic PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
target_link_libraries(test_cbloom_basic PRIVATE archbloom_shared)

add_executable(test_tdbloom_basic tests/test_tdbloom_basic.c)
set_target_properties(test_tdbloom_basic PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
target_link_libraries(test_tdbloom_basic PRIVATE archbloom_shared)

add_executable(test_tdcbloom_basic tests/test_tdcbloom_basic.c)
set_target_properties(test_tdcbloom_basic PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
target_link_libraries(test_tdcbloom_basic PRIVATE archbloom_shared)

add_executable(test_cuckoo_basic tests/test_cuckoo_basic.c)
set_target_properties(test_cuckoo_basic PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
target_link_libraries(test_cuckoo_basic PRIVATE archbloom_shared)

add_executable(test_gaussiannb_basic tests/test_gaussiannb_basic.c)
set_target_properties(test_gaussiannb_basic PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
target_link_libraries(test_gaussiannb_basic PRIVATE archbloom_shared)

# Install rules
install(TARGETS archbloom_shared archbloom_static
        LIBRARY DESTINATION lib)
install(FILES
    src/bloom.h
    src/mmh3.h
    src/cbloom.h
    src/tdbloom.h
    src/tdcbloom.h
    src/cuckoo.h
    src/gaussiannb.h
    DESTINATION include/archbloom)
install(CODE "execute_process(COMMAND ldconfig)")

# Testing
enable_testing()
add_test(NAME bloom COMMAND bin/test_bloom_basic)
add_test(NAME cbloom COMMAND bin/test_cbloom_basic)
add_test(NAME tdbloom COMMAND bin/test_tdbloom_basic)
add_test(NAME tdcbloom COMMAND bin/test_tdcbloom_basic)
add_test(NAME cuckoo COMMAND bin/test_cuckoo_basic)
add_test(NAME gaussiannb COMMAND bin/test_gaussiannb_basic)

# Doxygen
# To build documentation: cmake -DBUILD_DOC=ON
find_package(Doxygen)

if(DOXYGEN_FOUND)
    option(BUILD_DOC "Build documentation with Doxygen" OFF)
    if(BUILD_DOC)
	set(DOXYGEN_IN  ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
	set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

	configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

	add_custom_target(doc_doxygen ALL
	    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
	    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	    COMMENT "Generating documentation with Doxygen"
	    VERBATIM
	)

	# Install documentation
	install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION ${CMAKE_INSTALL_DOCDIR})
	# TODO compress manpages
	install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man DESTINATION ${CMAKE_INSTALL_MANDIR})

    endif()
else()
    message(STATUS "Doxygen not found. Not building documentation.")
endif()

# CLI tools
add_executable(bloomtool bin/bloomtool.c)
target_link_libraries(bloomtool PRIVATE archbloom_shared)
target_include_directories(bloomtool PRIVATE ${PROJECT_SOURCE_DIR}/src})
install(TARGETS bloomtool DESTINATION ${CMAKE_INSTALL_BINDIR})
