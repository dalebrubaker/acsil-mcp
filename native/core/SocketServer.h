// SocketServer.h — loopback TCP server: one thread accepts, one thread per client reads frames.
//
// Each request frame is passed to the handler on that client's thread, and the handler's return
// value is sent back as one reply frame. Binds 127.0.0.1 only.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace acsilmcp
{
    class SocketServer
    {
    public:
        using Handler = std::function<std::string(const std::string& requestPayload)>;
        using Logger = std::function<void(const std::string& message)>;

        SocketServer() = default;
        SocketServer(const SocketServer&) = delete;
        SocketServer& operator=(const SocketServer&) = delete;
        ~SocketServer();

        // Port 0 binds an ephemeral port; read it back with Port(). Returns false and fills `error`
        // if the socket cannot be bound.
        bool Start(std::uint16_t port, Handler handler, Logger logger, std::string& error);

        // Closes every socket and joins every thread. Safe to call when not started.
        void Stop();

        std::uint16_t Port() const { return port_; }
        bool IsRunning() const { return running_; }

    private:
        struct Client
        {
            std::uintptr_t socket;
            std::thread thread;
        };

        void AcceptLoop();
        void ClientLoop(std::uintptr_t socket);

        Handler handler_;
        Logger logger_;
        std::uintptr_t listenSocket_ = ~std::uintptr_t{0};
        std::uint16_t port_ = 0;
        std::atomic<bool> running_{false};
        std::thread acceptThread_;
        std::mutex clientsMutex_;
        std::vector<Client> clients_;
    };
}
