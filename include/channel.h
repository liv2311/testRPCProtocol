//channel.h
#pragma once
#include <vector>
#include <cstdint>
#include "physical_uart.h"

class Channel {
public:
    Channel(const std::string& uartDevice, int baudrate = B115200);

    bool open();
    void close();

    // Отправка сообщения транспортного уровня
    bool send(const std::vector<uint8_t>& payload);

    // Чтение одного пакета (если есть)
    std::vector<uint8_t> receive();

private:
    PhysicalUART uart;

    // буфер для приема
    std::vector<uint8_t> rxBuffer;

    uint8_t crc8(const std::vector<uint8_t>& data);
};
