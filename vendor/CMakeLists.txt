add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/SDL)

add_library(vendor INTERFACE)
target_link_libraries(vendor INTERFACE SDL3::SDL3)