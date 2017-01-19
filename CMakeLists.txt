cmake_minimum_required(VERSION 3.4.0)
project (kcpnet)
include_directories(./kcplib ./thirdparty)
aux_source_directory(./ DIRSRCS)

add_subdirectory(kcplib)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(kcpnet ${DIRSRCS} UdpServer.cpp UdpServer.h KcpServer.cpp KcpServer.h CFUtil.h kcplib/ikcp.h thirdparty/ProducerConsumerQueue.h)

target_link_libraries(kcpnet libkcp libglog.a libgflags.a libevent.a libevent_core.a libevent_extra.a libevent_pthreads.a Threads::Threads)

