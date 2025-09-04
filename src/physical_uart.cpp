// physical_uart.cpp
#include "physical_uart.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <iostream>

PhysicalUART::PhysicalUART(const std::string& device, int baudrate)
    : device(device), baudrate(baudrate) {}

PhysicalUART::~PhysicalUART() {
    closePort();
}

bool PhysicalUART::openPort() {
    fd = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return false;
    }

    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return false;
    }

    cfsetospeed(&tty, baudrate);
    cfsetispeed(&tty, baudrate);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return false;
    }
    return true;
}

void PhysicalUART::closePort() {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

bool PhysicalUART::send(const std::vector<uint8_t>& data) {
    if (fd < 0) return false;
    ssize_t written = ::write(fd, data.data(), data.size());
    return written == (ssize_t)data.size();
}

std::vector<uint8_t> PhysicalUART::receive() {
    std::vector<uint8_t> buf(256);
    if (fd < 0) return {};
    ssize_t n = ::read(fd, buf.data(), buf.size());
    if (n > 0) buf.resize(n);
    else buf.clear();
    return buf;
}
