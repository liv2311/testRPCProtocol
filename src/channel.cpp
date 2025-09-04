
//channel.cpp
#include "channel.h"
#include <iostream>

// Конструктор
Channel::Channel(const std::string& uartDevice, int baudrate)
    : uart(uartDevice, baudrate) {}

bool Channel::open() {
    return uart.openPort();
}

void Channel::close() {
    uart.closePort();
}

uint8_t Channel::crc8(const std::vector<uint8_t>& data) {
    uint8_t crc = 0;
    for (uint8_t b : data) {
        crc ^= b;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
    }
    return crc;
}

// Формат пакета:
// [0] = 0xFA
// [1] = len L
// [2] = len H
// [3] = crc8(header)
// [4] = 0xFB
// [5..n] = payload
// [n+1] = crc8(full)
// [n+2] = 0xFE
bool Channel::send(const std::vector<uint8_t>& payload) {
    uint16_t len = payload.size();
    std::vector<uint8_t> packet;
    packet.push_back(0xFA);
    packet.push_back(len & 0xFF);
    packet.push_back((len >> 8) & 0xFF);

    // crc по заголовку (байты 0-2)
    std::vector<uint8_t> hdr = {packet[0], packet[1], packet[2]};
    packet.push_back(crc8(hdr));

    packet.push_back(0xFB);
    packet.insert(packet.end(), payload.begin(), payload.end());

    uint8_t crcAll = crc8(packet);
    packet.push_back(crcAll);
    packet.push_back(0xFE);

    return uart.send(packet);
}

std::vector<uint8_t> Channel::receive() {
    auto data = uart.receive();
    if (!data.empty()) {
        rxBuffer.insert(rxBuffer.end(), data.begin(), data.end());
    }

    // Проверяем, есть ли полный пакет
    while (rxBuffer.size() >= 7) {
        if (rxBuffer[0] != 0xFA) {
            rxBuffer.erase(rxBuffer.begin()); // сдвигаем до стартового байта
            continue;
        }

        uint16_t len = rxBuffer[1] | (rxBuffer[2] << 8);
        size_t totalSize = 3 + 1 + 1 + len + 1 + 1; // header+crc+0xFB+payload+crc+0xFE

        if (rxBuffer.size() < totalSize) break; // ждем пока докачается

        // Проверка заголовка
        std::vector<uint8_t> header = {rxBuffer[0], rxBuffer[1], rxBuffer[2]};
        if (crc8(header) != rxBuffer[3]) {
            std::cerr << "Channel: Header CRC error\n";
            rxBuffer.erase(rxBuffer.begin());
            continue;
        }

        if (rxBuffer[4] != 0xFB || rxBuffer[totalSize - 1] != 0xFE) {
            std::cerr << "Channel: Format error\n";
            rxBuffer.erase(rxBuffer.begin());
            continue;
        }

        std::vector<uint8_t> packet(rxBuffer.begin(), rxBuffer.begin() + totalSize);

        // Проверка crc всего пакета
        uint8_t crcCalc = crc8(std::vector<uint8_t>(packet.begin(), packet.end() - 2));
        uint8_t crcRecv = packet[totalSize - 2];
        if (crcCalc != crcRecv) {
            std::cerr << "Channel: Packet CRC error\n";
            rxBuffer.erase(rxBuffer.begin());
            continue;
        }

        // Достаём payload
        std::vector<uint8_t> payload(packet.begin() + 5, packet.begin() + 5 + len);

        rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + totalSize);
        return payload;
    }

    return {};
}
