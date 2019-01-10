

project(tstlesrv)


set(EXECUTABLE_OUTPUT_PATH ../../)



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
    main.cpp)


target_link_libraries(${PROJECT_NAME} event muduo_net muduo_base)







