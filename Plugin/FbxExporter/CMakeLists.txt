find_package(FBX QUIET)
list(APPEND EXTERNAL_LIBS ${FBX_LIBRARY} dl)

file(GLOB FE_SOURCES *.cpp)
file(GLOB FE_HEADERS *.h)
set(FE_PLUGINS_DIR "${CMAKE_SOURCE_DIR}/../FbxExporter/Assets/UTJ/FbxExporter/Plugins/x86_64")
add_plugin(FbxExporterCore SOURCES ${FE_SOURCES} ${FE_HEADERS} PLUGINS_DIR ${FE_PLUGINS_DIR})

add_dependencies(FbxExporterCore MeshUtils)
target_include_directories(FbxExporterCore PRIVATE ${FBX_INCLUDE_DIR})
target_link_libraries(FbxExporterCore MeshUtils ${EXTERNAL_LIBS})
install(TARGETS FbxExporterCore DESTINATION .)
