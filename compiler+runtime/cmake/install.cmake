# When the compiler is installed, it needs to be relinked to its shared objects.
# We know where they'll be, relative to the compiler, though.
set_target_properties(jank_exe PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")

install(
  TARGETS jank_exe jank_cling_lib
  # Tempting to do this, to include all dynamic libs, but it doesn't reliably work.
  #RUNTIME_DEPENDENCIES DIRECTORIES
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(
  DIRECTORY ${jank_cling_build_dir}/lib/clang
  DESTINATION lib
)
install(
  PROGRAMS ${jank_cling_build_dir}/bin/clang++ ${jank_cling_build_dir}/bin/clang-13
  DESTINATION bin
)
install(
  PROGRAMS ${CMAKE_SOURCE_DIR}/bin/build-pch
  DESTINATION bin
)
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/src/jank
  DESTINATION src
)

# So vcpkg puts all of the headers we need for all of our dependencies
# into one include prefix based on our target triplet. This is excellent,
# since we need to package all of those headers along with jank. Why?
# Because JIT compilation means that any headers we need at compile-time
# we'll also need at run-time.
file(
  GLOB_RECURSE jank_includes
  ${CMAKE_SOURCE_DIR}/include/cpp/*
)
file(
  GLOB_RECURSE vcpkg_includes
  ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/*
)
file(
  GLOB_RECURSE third_party_includes
  ${CMAKE_SOURCE_DIR}/third-party/nanobench/include/*
  ${CMAKE_BINARY_DIR}/llvm/tools/cling/include/*
  ${CMAKE_BINARY_DIR}/llvm/tools/clang/include/*
  ${CMAKE_BINARY_DIR}/llvm/include/*
  ${jank_cling_build_dir}/tools/cling/include/*
  ${jank_cling_build_dir}/tools/clang/include/*
  ${jank_cling_build_dir}/include/*
)
set(jank_public_headers ${jank_includes} ${vcpkg_includes} ${third_party_includes})

# Normal installation doesn't keep directory information, so
# we do it ourselves.
macro(install_headers_with_directory header_list)
  foreach(header ${${header_list}})
    string(REGEX REPLACE ".*/(include/.*)[^/]*$" "\\1" relative_path ${header})
    cmake_path(GET relative_path PARENT_PATH relative_dir)
    install(FILES ${header} DESTINATION ${relative_dir})
  endforeach(header)
endmacro(install_headers_with_directory)

install_headers_with_directory(jank_public_headers)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
