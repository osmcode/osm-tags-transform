#------------------------------------------------------------------------------
#
#  src/CMakeLists.txt
#
#------------------------------------------------------------------------------

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/lua-init.lua")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/lua-init.lua" LUA_INIT_CODE)
configure_file(lua-init.cpp.in lua-init.cpp @ONLY)

add_executable(osm-tags-transform
    handler.cpp
    lua-utils.cpp
    main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/lua-init.cpp
)

target_link_libraries(osm-tags-transform PRIVATE ${LIBS})
target_link_libraries(osm-tags-transform PRIVATE Threads::Threads)

install(TARGETS osm-tags-transform DESTINATION bin)

#------------------------------------------------------------------------------
