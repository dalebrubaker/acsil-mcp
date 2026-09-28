#include "core/SocketServer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>

#include "core/Framing.h"

namespace acsilmcp
{
    namespace
    {
        bool SendAll(SOCKET socket, const std::string& data)
        {
            std::size_t sent = 0;
            while (sent < data.size())
            {
                const int chunk = static_cast<int>(std::min<std::size_t>(data.size() - sent, 1 << 20));
                const int n = send(socket, data.data() + sent, chunk, 0);
                if (n == SOCKET_ERROR)
                {
                    return false;
                }
                sent += static_cast<std::size_t>(n);
            }
            return true;
        }
    }

    SocketServer::~SocketServer()
    {
        Stop();
    }

    bool SocketServer::Start(std::uint16_t port, Handler handler, Logger logger, std::string& error)
    {
        if (running_)
        {
            error = "already running";
            return false;
        }

        WSADATA wsa{};
        if (const int rc = WSAStartup(MAKEWORD(2, 2), &wsa); rc != 0)
        {
            error = "WSAStartup failed: " + std::to_string(rc);
            return false;
        }

        const SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET)
        {
            error = "socket failed: " + std::to_string(WSAGetLastError());
            WSACleanup();
            return false;
        }

        // Refuse to share the port with another process.
        BOOL exclusive = TRUE;
        setsockopt(listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        if (bind(listenSocket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
            listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            error = "cannot listen on 127.0.0.1:" + std::to_string(port) + " (WSA error " +
                    std::to_string(WSAGetLastError()) + "); is the port in use?";
            closesocket(listenSocket);
            WSACleanup();
            return false;
        }

        sockaddr_in bound{};
        int boundLength = sizeof(bound);
        getsockname(listenSocket, reinterpret_cast<sockaddr*>(&bound), &boundLength);

        handler_ = std::move(handler);
        logger_ = std::move(logger);
        listenSocket_ = static_cast<std::uintptr_t>(listenSocket);
        port_ = ntohs(bound.sin_port);
        running_ = true;
        acceptThread_ = std::thread(&SocketServer::AcceptLoop, this);
        return true;
    }

    void SocketServer::Stop()
    {
        if (!running_.exchange(false))
        {
            return;
        }

        closesocket(static_cast<SOCKET>(listenSocket_));
        if (acceptThread_.joinable())
        {
            acceptThread_.join();
        }

        std::vector<Client> clients;
        {
            std::lock_guard lock(clientsMutex_);
            clients.swap(clients_);
        }
        for (auto& client : clients)
        {
            shutdown(static_cast<SOCKET>(client.socket), SD_BOTH);
            closesocket(static_cast<SOCKET>(client.socket));
        }
        for (auto& client : clients)
        {
            if (client.thread.joinable())
            {
                client.thread.join();
            }
        }

        WSACleanup();
        port_ = 0;
    }

    void SocketServer::AcceptLoop()
    {
        while (running_)
        {
            const SOCKET client = accept(static_cast<SOCKET>(listenSocket_), nullptr, nullptr);
            if (client == INVALID_SOCKET)
            {
                if (running_)
                {
                    logger_("accept failed: WSA error " + std::to_string(WSAGetLastError()));
                }
                return;
            }

            std::lock_guard lock(clientsMutex_);
            if (!running_)
            {
                closesocket(client);
                return;
            }
            const auto handle = static_cast<std::uintptr_t>(client);
            clients_.push_back(Client{handle, std::thread(&SocketServer::ClientLoop, this, handle)});
        }
    }

    void SocketServer::ClientLoop(std::uintptr_t handle)
    {
        const auto socket = static_cast<SOCKET>(handle);
        FrameDecoder decoder;
        char buffer[64 * 1024];
        while (running_)
        {
            const int n = recv(socket, buffer, sizeof(buffer), 0);
            if (n <= 0)
            {
                return; // client closed, or Stop() closed the socket
            }
            decoder.Append(buffer, static_cast<std::size_t>(n));

            try
            {
                while (auto request = decoder.Next())
                {
                    if (!SendAll(socket, EncodeFrame(handler_(*request))))
                    {
                        return;
                    }
                }
            }
            catch (const std::exception& ex)
            {
                logger_(std::string("dropping client: ") + ex.what());
                shutdown(socket, SD_BOTH);
                return;
            }
        }
    }
}
