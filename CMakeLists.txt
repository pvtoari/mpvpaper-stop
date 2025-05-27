cmake_minimum_required(VERSION 3.5)
project(mpvpaper-stop C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)

FetchContent_Declare(
	cjson
	GIT_REPOSITORY https://github.com/DaveGamble/cJSON
	GIT TAG v1.17.18
)

FetchContent_MakeAvailable(cjson)

add_executable(mpvpaper-stop src/main.c)

target_link_libraries(mpvpaper-stop PRIVATE cjson)
target_include_directories(mpvpaper-stop PRIVATE ${cjson_SOURCE_DIR})

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set_target_properties(mpvpaper-stop PROPERTIES
    INSTALL_RPATH "${CMAKE_INSTALL_RPATH}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

install(TARGETS mpvpaper-stop DESTINATION bin)
