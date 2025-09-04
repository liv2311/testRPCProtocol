#include <QCoreApplication>
#include <iostream>
#include <thread>
#include <atomic>
#include "transport.h"
#include "channel.h"

void registerFunctions(Transport& transport) {
    transport.registerFunction("sum", [](const std::vector<uint8_t>& args){
        if(args.size()<2) return std::vector<uint8_t>{0};
        return std::vector<uint8_t>{ uint8_t(args[0]+args[1]) };
    });
    transport.registerFunction("multiply", [](const std::vector<uint8_t>& args){
        if(args.size()<2) return std::vector<uint8_t>{0};
        return std::vector<uint8_t>{ uint8_t(args[0]*args[1]) };
    });
}

void runDevice(const std::string& port, const std::string& deviceName) {
    Channel channel(port, B115200);
    if(!channel.open()) {
        std::cerr << deviceName << ": Failed to open UART channel" << std::endl;
        return;
    }

    Transport transport(channel);
    registerFunctions(transport);
    transport.startAsyncReceive();

    std::atomic<bool> stopReader{false};
    std::thread reader([&]{
        while(!stopReader.load()) {
            auto packet = channel.receive();
            if(!packet.empty()) transport.incomingQueue.push(packet);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Поток для отправки RPC-запросов
    std::thread sender([&]{
        auto r1 = transport.sendRequest("sum",{3,4},5000);
        if(!r1.empty()) std::cout << deviceName << ": sum(3,4) = " << int(r1[0]) << std::endl;

        auto r2 = transport.sendRequest("multiply",{5,6},5000);
        if(!r2.empty()) std::cout << deviceName << ": multiply(5,6) = " << int(r2[0]) << std::endl;

        auto r3 = transport.sendRequest("unknown",{1,2},5000);
        if(r3.empty()) std::cout << deviceName << ": Function 'unknown' returned error" << std::endl;
    });

    sender.join();
    stopReader = true;
    if(reader.joinable()) reader.join();

    transport.stopAsyncReceive();
    channel.close();
    std::cout << deviceName << ": UART closed" << std::endl;
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    std::cout << "Main: Starting RPC test on virtual ports" << std::endl;

    std::thread deviceA([]{ runDevice("/tmp/ttyV0", "Dev1"); });
    std::thread deviceB([]{ runDevice("/tmp/ttyV1", "Dev2"); });

    deviceA.join();
    deviceB.join();

    std::cout << "Main: RPC test finished" << std::endl;
    return 0;
}
