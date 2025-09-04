#include "transport.h"
#include <iostream>
#include <chrono>

Transport::Transport(Channel& ch) : channel(ch) {}

int Transport::generateRequestId() {
    return ++nextRequestId;
}

void Transport::registerFunction(const std::string &name, RPCFunction func) {
    functions[name] = func;
}

std::vector<uint8_t> Transport::encodeMessage(uint8_t type, int id,
                                              const std::string &name,
                                              const std::vector<uint8_t> &args) {
    std::vector<uint8_t> msg;
    msg.push_back(type);
    msg.push_back(static_cast<uint8_t>(id & 0xFF));
    msg.insert(msg.end(), name.begin(), name.end());
    msg.push_back(0);
    msg.insert(msg.end(), args.begin(), args.end());
    return msg;
}

bool Transport::decodeMessage(const std::vector<uint8_t>& msg,
                              uint8_t &type,
                              int &id,
                              std::string &name,
                              std::vector<uint8_t> &args) {
    if(msg.size() < 3) return false;
    type = msg[0];
    id = msg[1];
    size_t i = 2;
    name.clear();
    while(i < msg.size() && msg[i] != 0) name += static_cast<char>(msg[i++]);
    if(i >= msg.size()) return false;
    i++;
    args.assign(msg.begin()+i, msg.end());
    return true;
}

std::vector<uint8_t> Transport::sendRequest(const std::string &name,
                                            const std::vector<uint8_t> &args,
                                            int timeoutMs) {
    int reqId = generateRequestId();
    PendingRequest pr(reqId);

    {
        std::lock_guard<std::mutex> lock(pendingMtx);
        pendingRequests[reqId] = &pr;
    }

    channel.send(encodeMessage(static_cast<uint8_t>(MessageType::REQUEST), reqId, name, args));

    std::unique_lock<std::mutex> lock(pr.mtx);
    bool received = pr.conditionVariable.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                   [&pr]{ return pr.completed; });

    std::vector<uint8_t> resp;
    if(received) resp = pr.response;
    else std::cerr << "Transport: Request ID " << reqId << " timed out" << std::endl;

    {
        std::lock_guard<std::mutex> lock(pendingMtx);
        pendingRequests.erase(reqId);
    }

    return resp;
}

void Transport::startAsyncReceive() {
    stopFlag = false;
    processorThread = std::thread([this]{ processIncoming(); });
}

void Transport::stopAsyncReceive() {
    stopFlag = true;
    incomingQueue.push({}); // разблокировать ожидание
    if(processorThread.joinable()) processorThread.join();
}

void Transport::processIncoming() {
    while(!stopFlag) {
        auto packet = incomingQueue.wait_and_pop();
        if(packet.empty()) continue;

        uint8_t type;
        int id;
        std::string name;
        std::vector<uint8_t> args;

        if(!decodeMessage(packet, type, id, name, args)) continue;

        if(type == static_cast<uint8_t>(MessageType::REQUEST)) {
            uint8_t respType = static_cast<uint8_t>(MessageType::RESPONSE);
            std::vector<uint8_t> result;
            auto it = functions.find(name);
            if(it != functions.end()) result = it->second(args);
            else respType = static_cast<uint8_t>(MessageType::ERROR);
            channel.send(encodeMessage(respType, id, name, result));
        } else if(type == static_cast<uint8_t>(MessageType::RESPONSE) ||
                  type == static_cast<uint8_t>(MessageType::ERROR)) {
            PendingRequest* pr = nullptr;
            {
                std::lock_guard<std::mutex> lock(pendingMtx);
                auto it = pendingRequests.find(id);
                if(it != pendingRequests.end()) pr = it->second;
            }
            if(pr) {
                std::lock_guard<std::mutex> lock(pr->mtx);
                pr->response = args;
                pr->completed = true;
                pr->conditionVariable.notify_one();
            }
        } else if(type == static_cast<uint8_t>(MessageType::STREAM)) {
            std::cout << "Transport: Stream received: ";
            for(auto b : args) std::cout << int(b) << " ";
            std::cout << std::endl;
        }
    }
}
