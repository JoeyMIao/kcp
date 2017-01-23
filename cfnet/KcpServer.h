//
// Created by joeymiao on 17-1-19.
//

#ifndef KCPNET_KCPSERVER_H
#define KCPNET_KCPSERVER_H

#include "UdpServer.h"
#include "ikcp.h"
#include <string>

namespace cf
{
    class KcpServer
    {
    public:
        KcpServer() : _started(false),_running(false), _curSessionSeq(0), _curUdpBuffIndex(0), _udpServer(nullptr) {}

        bool start(const std::string &ip, const uint16_t port, uint32_t guessSize)
        {
            assert(!_started);
            _udpServer = new UdpServer(ip, port, guessSize);
            assert(_udpServer);
            if(!_udpServer->init())
                return false;
            _udpThread = std::thread([&] { _udpServer->start();});
            _started = true;
            _running = true;
            return true;
        }

        void shutdown()
        {
            _udpServer->shutdown();
            _udpThread.join();
            for(auto iter = _idSessionMap.begin();iter!=_idSessionMap.end();++iter)
            {
                assert(iter->second);
                delete iter->second;
            }
            _idSessionMap.clear();
            _addrSessionMap.clear();
        }

        void update(uint32_t curTick)
        {
            if (!_started ||!_running)
                return;

            UdpServer::Message mess;
            while (_udpServer->tryPopValue(mess))
            {
                auto msgSessionIter = _addrSessionMap.find(mess.addr);
                if (msgSessionIter != _addrSessionMap.end())
                {
                    handleRawMessage(mess, msgSessionIter->second->kcp);
                }
                else
                {
                    if(validNewConnectionMsg(mess))
                    {
                        LOG(INFO) << "OnConnected:" << _curSessionSeq + 1;
                        processNewConnection(mess);
                    }
                    else
                    {
                        LOG(INFO)<<"recv unconnected msg size:"<< mess.buffSize;
                    }
                }

                recycleBuffToUdpServer(mess);
            }

            auto iterBegin = _idSessionMap.begin();
            auto iterEnd = _idSessionMap.end();
            for (; iterBegin != iterEnd; iterBegin++)
            {
                if (iterBegin->second->kcp)
                {
                    ikcp_update(iterBegin->second->kcp, curTick);
                }
            }
        }

        bool sendtoBySessionID(const uint32_t sessionID, const char *buff, int32_t size) const
        {
            auto iter = _idSessionMap.find(sessionID);
            if (_idSessionMap.end() == iter)
            {
                return false;
            }
            ssize_t ret = _udpServer->sendMessageTo(iter->second->sockAddr, buff, size);

            //TODO:needs an onDisconnect callback
            return ret >= 0;

        }

        bool sendtoByAddr(struct sockaddr_in &addr, const char *buff, int32_t size) const
        {
            ssize_t ret = _udpServer->sendMessageTo(addr, buff, size);

            //TODO:needs an onDisconnect callback
            return ret >= 0;
        }

        uint32_t removeSessionIfExsitsByAddr(const sockaddr_in &addr)
        {
            auto iter = _addrSessionMap.find(addr);
            if (_addrSessionMap.end() == iter)
                return 0;
            uint32_t sessionID = iter->second->sessionID;
            int erased = (int) _idSessionMap.erase(sessionID);
            _addrSessionMap.erase(iter);
            assert(erased > 0);
            return sessionID;
        }

        uint32_t removeSessionIfExsitsByInt(const uint32_t sessionID)
        {
            auto iter = _idSessionMap.find(sessionID);
            if (iter == _idSessionMap.end())
                return 0;
            int erased = (int) _addrSessionMap.erase(iter->second->sockAddr);
            _idSessionMap.erase(iter);
            assert(erased > 0);
            return sessionID;
        }


    private:

        static int udpOutput(const char *buf, int len, ikcpcb *kcp, void *user)
        {
            KcpServer *kcpServer = static_cast<KcpServer *>(user);
            kcpServer->sendtoBySessionID(kcp->conv, buf, len);
            return 0;
        }

        void handleRawMessage(const UdpServer::Message& mess, ikcpcb *kcp)
        {
            ikcp_input(kcp, mess.buff, mess.buffSize);
            while (true)
            {
                int gotSize = ikcp_recv(kcp, _buff, BuffSize);
                if (gotSize <= 0)
                    break;
                LOG(INFO) << "recv:" << std::string(_buff, (unsigned long) gotSize);
                ikcp_send(kcp, _buff, gotSize);
            }
        }

        bool validNewConnectionMsg(const UdpServer::Message& mess) const
        {
            if(mess.buffSize==4)
            {
                uint32_t magicCode;
                memcpy(&magicCode, mess.buff, 4);
                if (magicCode == 100319)
                {
                    return true;
                }
            }
            return false;
        }

        void processNewConnection(const UdpServer::Message& mess)
        {
            Session *session = new Session(++_curSessionSeq, mess.addr);
            assert(session);
            session->kcp = ikcp_create((IUINT32) _curSessionSeq, (void *) this);
            assert(session->kcp);
            session->kcp->output = udpOutput;
            _addrSessionMap.insert(std::make_pair(mess.addr, session));
            _idSessionMap.insert(std::make_pair(_curSessionSeq, session));
            memcpy(mess.buff, &_curSessionSeq,  4);
            _udpServer->sendMessageTo(mess.addr, mess.buff, mess.buffSize);
        }

        void recycleBuffToUdpServer(const UdpServer::Message& mess)
        {
            if(mess.buffSize+_curUdpBuffIndex >= _udpServer->prelocatedBuffBlockSize)
            {
                //TODO:错误处理
                _udpServer->returnBuff(mess.buff);
                _curUdpBuffIndex = 0;
            }
            else
            {
                _curUdpBuffIndex += mess.buffSize;
            }
        }

        struct SockAddrComp
        {
            bool operator()(const struct sockaddr_in &lhs, const struct sockaddr_in &rhs) const
            {
                return memcmp(&lhs, &rhs, sizeof(const struct sockaddr_in &)) < 0;
            }
        };

        struct Session
        {
            Session(uint32_t id, const sockaddr_in &addr) :
                    sessionID(id),
                    sockAddr(addr),
                    kcp(nullptr) {}

            ~Session()
            {
                if (kcp)
                    ikcp_release(kcp);
            }

            uint32_t sessionID;
            sockaddr_in sockAddr;
            ikcpcb *kcp;
        };

        const static uint32_t MagicNum = 100319;
        const static int32_t BuffSize = 1024 * 64;

        bool _started;
        bool _running;
        uint32_t _curSessionSeq;
        char _buff[BuffSize];

        std::thread _udpThread;
        int32_t _curUdpBuffIndex;
        UdpServer *_udpServer;
        std::map<struct sockaddr_in, Session *, SockAddrComp> _addrSessionMap;
        std::map<uint32_t, Session *> _idSessionMap;
    };
};

#endif //KCPNET_KCPSERVER_H
