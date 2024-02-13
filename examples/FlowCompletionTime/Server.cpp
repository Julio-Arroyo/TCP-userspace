#include "JcTcpSocket.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>

#define BUF_SIZE 2048

int main() {
  JC::TcpSocket sock;
  sock.open(JC::SocketType::TCP_LISTENER,
            SERVER_PORT, SERVER_IP_ADDR);
  
  std::string fname = "./output/received_file.txt";
  std::ofstream f{fname};
  if (!f.is_open()) {
    std::cerr << "Could not create output file." << fname
              << std::endl;
  }

  char buf[BUF_SIZE];
  while (true) {
    int read_len = sock.read(static_cast<void*>(buf),
                             BUF_SIZE,
                             JC::ReadMode::BLOCK);
    for (int i = 0; i < read_len; i++) {
      f << buf[i];
    }

    if (read_len == 0) {
      break;
    }
  }

  sock.teardown();
  auto end_time = std::chrono::high_resolution_clock::now().time_since_epoch();
  std::cout << "End time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_time).count()
            << std::endl;
  // std::time_t end_time = std::chrono::high_resolution_clock::to_time_t(std::chrono::high_resolution_clock::now());
  // std::cout << "End time: " << std::ctime(&end_time) << std::endl;

  f.close();

  return 0;
}

