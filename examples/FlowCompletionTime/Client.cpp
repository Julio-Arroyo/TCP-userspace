#include "JcTcpSocket.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: ./flow_completion_time [FILE_SIZE]" << std::endl;
    return -1;
  }
  
  std::string file_size{argv[1]};
  std::string fname = "./files/file_" + file_size + ".txt";
  std::ifstream f{fname};
  if (!f.is_open()) {
    std::cerr << "Could not open file " << fname << std::endl;
    return -1;
  }

  auto start_time = std::chrono::high_resolution_clock::now().time_since_epoch();
  std::cout << "Start time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(start_time).count()
            << std::endl;
  // std::time_t start_time = std::chrono::high_resolution_clock::to_time_t(std::chrono::high_resolution_clock::now());
  // std::cout << "Start time: " << std::ctime(&start_time) << std::endl;
  JC::TcpSocket sock;
  sock.open(JC::SocketType::TCP_INITIATOR,
            SERVER_PORT,
            SERVER_IP_ADDR);

  char buf[MAX_PAYLOAD_SIZE];
  size_t bytes_read{0};
  while (true) {
    f.read(buf, MAX_PAYLOAD_SIZE);
    size_t curr_bytes_read = f.gcount();

    sock.write(static_cast<void*>(buf), curr_bytes_read);

    bytes_read += curr_bytes_read;
    if (curr_bytes_read < MAX_PAYLOAD_SIZE) {
      break;
    }
  }
  std::cout << "Total bytes read from file: "
            << bytes_read << std::endl;

  sock.teardown();
  f.close();

  return 0;
}

