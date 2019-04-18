

project(tstlesrv)


set(EXECUTABLE_OUTPUT_PATH ../../)


add_definitions(

    -DH_USE_BLOCKINGQUEUE

#    -DH_HAVE_CAMERON314_CONCURRENTQUEUE
#    -DH_HAVE_BOOST
)
message("==> aaaaaaaaaaaaaaaaaaaaaaa")
message(${EXECUTABLE_OUTPUT_PATH})
message("==> bbbbbbbbbbbbbbbbbbbbbbb")


add_executable(${PROJECT_NAME}
    tstsrv.h tstsrv.cpp
    tcpserver.h tcpserver.cpp
    eventloopthread.h eventloopthread.cpp
    iothreadmanager.hpp iothreadmanager.cpp
    common_stucts.hpp common_stucts.cpp
    iothread.h iothread.cpp
    clientctx.hpp clientctx.cpp
    sigslot.h sigslot.cc
    basetransport.h basetransport.cpp
    frametransport.h frametransport.cpp
    tstthreadpool.h tstthreadpool.cpp
    taskthreadpool.h taskthreadpool.cpp
    concurrentqueue.h concurrentqueue.cpp
    main.cpp)


target_link_libraries(${PROJECT_NAME} event muduo_net muduo_base)







