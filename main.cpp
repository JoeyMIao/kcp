//
// Created by joeymiao on 17-1-14.
//

#include <stdio.h>
#include "KcpServer.h"

int main(int argc, char* argv[])
{
    //google::InitGoogleLogging(argv[0]);
    //google::ShutdownGoogleLogging();
    //UdpServer server("127.0.0.1",13333);
    //server.start();

    cf::KcpServer kcpServer;
    kcpServer.start("127.0.0.1", 13333, 5000);

    return 0;
}