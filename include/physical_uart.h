// physical_uart.h
#pragma once
#include <string>
#include <vector>
#include <termios.h>

class PhysicalUART {
public:
    PhysicalUART(const std::string& device, int baudrate = B115200);
    ~PhysicalUART();

    bool openPort();
    void closePort();

    bool send(const std::vector<uint8_t>& data);
    std::vector<uint8_t> receive();

private:
    std::string device;
    int baudrate;
    int fd = -1;
    struct termios tty{};
};
