cmake_minimum_required(VERSION 3.5)

project(tim-cli)

add_library(tim_io_lib STATIC tim_io_utils.c)

# TODO: set this for win/linux, or just include argp source
set(ARGP_PATH /opt/homebrew/opt/argp-standalone)
add_executable(timpack timpack.c)
target_compile_options(timpack PRIVATE -Wall -Werror)
target_include_directories(timpack PRIVATE ${ARGP_PATH}/include)
target_link_libraries(timpack PRIVATE tim_io_lib ${ARGP_PATH}/lib/libargp.a)

find_package(SDL2 REQUIRED)
message(STATUS "SDL2 includes from ${SDL2_INCLUDE_DIRS}")

add_executable(timview timview.c)
target_compile_options(timview PRIVATE -Wall -Werror)
target_include_directories(timview PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(timview PRIVATE tim_io_lib ${SDL2_LIBRARIES})

