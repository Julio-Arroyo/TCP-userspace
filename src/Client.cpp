#include "JcTcpSocket.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

void transferFile(const std::string& fname) {
  std::ifstream fileToTransfer{fname};
  if (!fileToTransfer.is_open()) {
    std::cerr << "Could not open file " << fname << std::endl;
    return;
  }

  std::stringstream buf;
  buf << fileToTransfer.rdbuf();
  fileToTransfer.close();
  std::string contents = buf.str();
  std::cout << "Transmitted file size: " << contents.length() << std::endl;

  JC::TcpSocket sock;
  sock.open(JC::SocketType::TCP_INITIATOR, SERVER_PORT, SERVER_IP_ADDR);
  sock.write(static_cast<void*>(contents.data()), contents.length());
  sock.teardown();
}

void transmitNumberSequence(const int sequence_len) {
  JC::TcpSocket sock;

  // connect to server
  sock.open(JC::SocketType::TCP_INITIATOR,
            SERVER_PORT,
            SERVER_IP_ADDR);

  char buf[sequence_len];
  char epoch = 0;
  std::cout << (int) epoch << std::endl;
  int count = 0;
  while (count < sequence_len) {
    buf[count++] = epoch;
    if (count % 256 == 0) {
      epoch++;
    }
  }
  std::cout << (int) epoch << std::endl;

  sock.write(static_cast<void*>(buf), sequence_len);

  sock.teardown();
}

int main() {
  transferFile("../inc/JcTcpSocket.hpp");

  // transmitNumberSequence(1800);

  return 0;
}

