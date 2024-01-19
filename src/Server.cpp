#include "JcTcp.hpp"

#include <iostream>

#define BUF_SIZE 256

int main() {
  JC::TcpSocket sock;
  sock.open(JC::SocketType::TCP_LISTENER, SERVER_PORT, SERVER_IP_ADDR);

  JC::TcpHeader dummyHeader;
  std::cout << "sizeof(JC::TcpHeader):" << sizeof(dummyHeader) << std::endl;

  char msg[BUF_SIZE];
  int msg_size = sock.read(static_cast<void*>(msg), BUF_SIZE, JC::ReadMode::BLOCK);

  std::cout << "Client message: " << std::endl;
  for (int i = 0; i < msg_size; i++) {
    std::cout << msg[i];
  }
  std::cout << std::endl;

  sock.teardown();

  return 0;
}
