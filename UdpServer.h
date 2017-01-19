//
// Created by joeymiao on 17-1-16.
//

#ifndef KCPNET_KCPUDPSERVER_H
#define KCPNET_KCPUDPSERVER_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <thread>
#include <assert.h>
#include <glog/logging.h>
#include <map>

#include "ProducerConsumerQueue.h"

#include "event.h"

namespace cf
{
    class UdpServer
    {
    public:
        struct Message
        {
            Message(uint16_t s, char *buf, const sockaddr_in add)
                    : buffSize(s), buff(buf), addr(add) {}

            Message() {}

            uint16_t buffSize;
            char *buff;
            sockaddr_in addr;
        };

        UdpServer(const std::string &ip, const uint16_t port, uint32_t guessSize) :
                _started(false), _ip(ip), _port(port), _base(event_base_new()), _listenSocket(0),
                curInUseBuf(nullptr), curBuffIndex(0),extraMallocBuffCount(0),
                _readMsgQueue(guessSize), _bufferQueue(PrelocatedBuffBlockCount * 10)
        {

        }

        ~UdpServer()
        {
            if (_base != nullptr)
                event_base_free(_base);

            if (_listenSocket != 0)
                evutil_closesocket(_listenSocket);
        }

        bool start()
        {
            if(!initBuffer())
                return false;
            _listenSocket = socket(AF_INET, SOCK_DGRAM, 0);
            assert(_listenSocket > 0);
            int result = evutil_make_listen_socket_reuseable(_listenSocket);
            LOG(INFO) << "Reuse result " << result;
            result = evutil_make_socket_nonblocking(_listenSocket);
            LOG(INFO) << "Nonblocking Result " << result;

            struct sockaddr_in sin;
            sin.sin_family = AF_INET;
            sin.sin_addr.s_addr = ::inet_addr(_ip.c_str());
            sin.sin_port = htons(_port);
            if (bind(_listenSocket, (const struct sockaddr *) &sin, sizeof(sin)) != 0)
            {
                LOG(INFO) << "Bind Error !";
                return false;
            }

            event *listenEvent = event_new(_base, _listenSocket, EV_READ | EV_PERSIST, eventCallback, this);
            event_add(listenEvent, NULL);
            event_base_dispatch(_base);
            LOG(INFO) << "Server started";
            _started = true;
            return true;
        }

        ssize_t sendMessageTo(const sockaddr_in &addr, const char *buff, int32_t size)
        {
            return sendto(_listenSocket, buff, (size_t) size, 0, (struct sockaddr *) &addr,
                          (socklen_t) sizeof(sockaddr_in));
        }

        bool tryPopValue(Message& msg)
        {
            return _readMsgQueue.read(msg);
        }

        bool returnBuff(char* buf)
        {
            bool result = _bufferQueue.write(buf);
            if(!result)
            {
                LOG(FATAL)<<"buffer return to udpserver failed";
            }
            return result;
        }

        const static socklen_t AddrLen = sizeof(struct sockaddr_in);
        const static size_t MAXMTU = 1500;
        const static int32_t PrelocatedBuffBlockSize = 1024 * 1000;
        const static int32_t PrelocatedBuffBlockCount = 20;

    private:
        static void eventCallback(evutil_socket_t sock, short evFlags, void *serverPtr)
        {
            UdpServer *server = static_cast<UdpServer *>(serverPtr);
            if (evFlags & EV_READ)
            {
                server->checkReadBuffer();
                socklen_t len = AddrLen;
                Message mess;
                //TODO: try to recv until an EINTER happen
                ssize_t size = recvfrom(sock, server->curInUseBuf, MAXMTU, 0, (struct sockaddr *) &mess.addr,
                                        &len);
                if (size == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                        return;
                    else
                    {
                        LOG(INFO) << "read error " << errno << ", from " << inet_ntoa(mess.addr.sin_addr)
                                  << ":"
                                  << mess.addr.sin_port;
                    }
                }
                else
                {
                    LOG(INFO) << "read from " << inet_ntoa(mess.addr.sin_addr) << ":"
                              << ::ntohs(mess.addr.sin_port)
                              << "," << std::string(server->curInUseBuf, (unsigned long) size);

                    mess.buffSize = (uint16_t) size;
                    mess.buff = server->curInUseBuf;
                    server->curBuffIndex += size;
                    //TODO:if no msgQueue is full, then stop reciving message
                    if(!server->_readMsgQueue.write(mess))
                    {
                        LOG(FATAL) <<"read msg queue is full,discard msg";
                    }
                }

            } else if (evFlags & EV_TIMEOUT)
            {
                LOG(INFO) << "time out";
            }
        }

        inline void checkReadBuffer()
        {
            if(PrelocatedBuffBlockSize - curBuffIndex < MAXMTU)
            {
                char* newBuff;
                bool result = _bufferQueue.read(newBuff);
                if(result)
                {
                    curInUseBuf = newBuff;
                    curBuffIndex = 0;
                }
                else
                {
                    //TODO:if no left buffer,try allocate a mtu buffer one time, avoid OOM
                    newBuff = (char*)malloc(PrelocatedBuffBlockSize);
                    if(!newBuff)
                    {
                        LOG(FATAL) << "allocate new buff failed ";
                        assert(newBuff!=nullptr);
                        curInUseBuf = newBuff;
                        curBuffIndex = 0;
                    }
                    extraMallocBuffCount++;
                }
            }
        }

        bool initBuffer()
        {
            //prelocate 20M memory for buffer
            for (int j = 0; j < PrelocatedBuffBlockCount; ++j)
            {
                void *buffer = malloc(PrelocatedBuffBlockSize);
                if(!buffer)
                {
                    assert(buffer != nullptr);
                    return false;
                }
                _bufferQueue.write((char *) buffer);
            }
            if(!_bufferQueue.read(curInUseBuf))
            {
                LOG(FATAL)<<"init buff error";
                assert(true);
                return false;
            }
            return true;
        }

        bool _started;
        const std::string _ip;
        const uint16_t _port;
        event_base *const _base;
        evutil_socket_t _listenSocket;
        char *curInUseBuf;
        uint32_t curBuffIndex;
        uint32_t extraMallocBuffCount;
        //self as a message producer
        ProducerConsumerQueue<Message> _readMsgQueue;
        //self as a buffer consumer
        ProducerConsumerQueue<char *> _bufferQueue;

    };

};
#endif //KCPNET_KCPUDPSERVER_H
