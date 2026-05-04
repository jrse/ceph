# librdkafka - build it statically from submodule
function(build_rdkafka)
  set(RDKAFKA_C_FLAGS "-fPIC -O2")

  include(ExternalProject)
  ExternalProject_Add(rdkafka_ext
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/librdkafka
    CMAKE_ARGS -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
               -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
               -DCMAKE_C_FLAGS=${RDKAFKA_C_FLAGS}
               -DCMAKE_AR=${CMAKE_AR}
               -DCMAKE_POSITION_INDEPENDENT_CODE=ON
               -DRDKAFKA_BUILD_STATIC=ON
               -DRDKAFKA_BUILD_EXAMPLES=OFF
               -DRDKAFKA_BUILD_TESTS=OFF
               -DWITH_SSL=ON
               -DWITH_SASL=ON
               -DWITH_ZSTD=OFF
               -DWITH_ZLIB=OFF
               -DWITH_LZ4_EXT=OFF
               -DENABLE_LZ4_EXT=OFF
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/librdkafka
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target rdkafka
    BUILD_BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/librdkafka/src/librdkafka.a"
    INSTALL_COMMAND "")

  add_library(RDKafka::RDKafka STATIC IMPORTED GLOBAL)
  set_target_properties(RDKafka::RDKafka PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src/librdkafka/src"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/librdkafka/src/librdkafka.a"
    INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto")
  add_dependencies(RDKafka::RDKafka rdkafka_ext)
endfunction()
