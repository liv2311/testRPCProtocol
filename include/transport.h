#pragma once
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include "channel.h"
#include "async_queue.h"

using RPCFunction = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

class Transport {
public:
    Transport(Channel& ch);

    void registerFunction(const std::string &name, RPCFunction func);

    // клиентский вызов RPC
    std::vector<uint8_t> sendRequest(const std::string &name,
                                     const std::vector<uint8_t> &args,
                                     int timeoutMs = 1000);

    // асинхронный приём
    void startAsyncReceive();
    void stopAsyncReceive();

    enum class MessageType : uint8_t {
        REQUEST = 0x0B,
        STREAM  = 0x0C,
        RESPONSE= 0x16,
        ERROR   = 0x21
    };

    AsyncQueue<std::vector<uint8_t>> incomingQueue;

private:
    void processIncoming();

    struct PendingRequest {
        int id;
        std::vector<uint8_t> response;
        bool completed = false;
        std::mutex mtx;
        std::condition_variable conditionVariable;

        PendingRequest(int i) : id(i) {}
        PendingRequest(const PendingRequest&) = delete;
        PendingRequest& operator=(const PendingRequest&) = delete;
    };

    Channel& channel;
    std::atomic<bool> stopFlag{false};
    std::thread processorThread;

    int nextRequestId = 0;
    int generateRequestId();

    std::vector<uint8_t> encodeMessage(uint8_t type, int id,
                                       const std::string &name,
                                       const std::vector<uint8_t> &args);
    bool decodeMessage(const std::vector<uint8_t>& msg, uint8_t &type,
                       int &id, std::string &name, std::vector<uint8_t> &args);

    std::map<std::string, RPCFunction> functions;
    std::map<int, PendingRequest*> pendingRequests;
    std::mutex pendingMtx;
};
