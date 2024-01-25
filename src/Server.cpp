#include "JcTcp.hpp"

#include <iostream>
#include <limits>

#define BUF_SIZE 2048
#define MAX_ATTEMPTS 100

void receiveFile() {
  JC::TcpSocket sock;
  sock.open(JC::SocketType::TCP_LISTENER, SERVER_PORT, SERVER_IP_ADDR);

  char line[BUF_SIZE];
  int file_size = 0;
  while (true) {
    int read_size = sock.read(static_cast<void*>(line), BUF_SIZE, JC::ReadMode::BLOCK);
    file_size += read_size;
    std::string line_str(line);
    std::cout << line_str.substr(0, read_size);

    // std::cout << "line_len " << file_size << std::endl;
  }
  // size_t attempts = 0;
  // while (line_len > 0 || attempts++ < MAX_ATTEMPTS) {
  //   std::cout << line << std::endl;
  //   line_len = sock.read(static_cast<void*>(line), BUF_SIZE, JC::ReadMode::NO_WAIT);
  // }
  sock.teardown();
}

void receiveNumberSequence() {
  JC::TcpSocket sock;
  sock.open(JC::SocketType::TCP_LISTENER, SERVER_PORT, SERVER_IP_ADDR);

  std::string dummyStr;
  std::cout << "type something to continue" << std::endl;
  std::cin >> dummyStr;

  char msg[BUF_SIZE];
  int msg_size = sock.read(static_cast<void*>(msg), BUF_SIZE, JC::ReadMode::BLOCK);
  std::cout << "msg size" << msg_size << std::endl;
  std::cout << "Client message: " << std::endl;
  for (int i = 0; i < msg_size; i++) {
    int num = msg[i];

    std::cout << (255*num + num) << ","; 
  }
  std::cout << std::endl;

  sock.teardown();
}

int main() {
  receiveFile();

  // receiveNumberSequence();
  return 0;
}
