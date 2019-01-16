cmake_minimum_required(VERSION 2.8)

project(newframework C CXX)


set(CXX_FLAGS
    -g
    # -DVALGRIND
     -DMUDUO_STD_STRING
    -DCHECK_PTHREAD_RETURN_VALUE
    -D_FILE_OFFSET_BITS=64
    -Wall
    -Wextra
    -Werror
    -Wconversion
    -Wno-unused-parameter
    -Wold-style-cast
    -Woverloaded-virtual
    -Wpointer-arith
    -Wshadow
    -Wwrite-strings
    -march=native
    # -MMD
    # -std=c++0x
    -std=c++11
    -rdynamic
    )

file(GLOB_RECURSE CURRENT_HEADERS *.h *.hpp *.cpp)
source_group("Include" FILES ${CURRENT_HEADERS})


option( _debug "Debug option" ON )


include_directories("${CMAKE_SOURCE_DIR}")
include_directories(${CMAKE_SOURCE_DIR}/thirdpart/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/thirdpart/thrift/thrift-0.9.3)
include_directories(${CMAKE_SOURCE_DIR}/thirdpart/thrift/thrift-0.9.3/lib/cpp/src)


aux_source_directory(. SRC_LIST)
aux_source_directory(./muduo/base BASE_LIST)
aux_source_directory(./muduo/net  NET_LIST)
aux_source_directory(./muduo/net/poller  POLLER_LIST)
#aux_source_directory(./poco              POCO_LIST)

#add_executable(${PROJECT_NAME} ${SRC_LIST} ${BASE_LIST}  ${NET_LIST}  ${POLLER_LIST} )

link_directories(${LIBRARY_OUTPUT_PATH})
#link_directories(/usr/lib)
link_directories(/usr/local/lib)
link_directories(/opt/openssl/lib/)



add_subdirectory(thirdpart/thrift)
add_subdirectory(muduo/base)
add_subdirectory(muduo/net)

add_subdirectory(examples)

message("==> CMAKE_SOURCE_DIR")
message(${CMAKE_SOURCE_DIR})
message("==> CMAKE_SOURCE_DIR")




#target_link_libraries(${PROJECT_NAME} uuid pthread log4cplus boost_thread boost_system boost_filesystem )
#target_link_libraries(${PROJECT_NAME} boost_system rt mysqlcppconn )


# if(_debug)
#     message("here, build debug version")
#     add_definitions(-D_DEBUG_)
# #-std=c++11
#     set(CMAKE_BUILD_TYPE Debug)
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g -Wno-deprecated  -std=c++11")
# #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g -Wno-deprecated ")
# else()
#     message("here, build release version")
#     set(CMAKE_BUILD_TYPE Release)
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -O2 -Wno-deprecated -std=c++11")
# endif()





