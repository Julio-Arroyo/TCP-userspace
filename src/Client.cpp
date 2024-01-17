#include "JcTcp.hpp"

#include <iostream>
#include <string>

int main() {
  JC::TcpSocket sock;

  // connect to server
  sock.open(JC::SocketType::TCP_INITIATOR,
            SERVER_PORT,
            SERVER_IP_ADDR);

  // block so that server has time to startu up
  std::cout << "Waiting for server... type something to continue: " << std::endl;
  std::string dummyBlock;
  std::cin >> dummyBlock;

  sock.teardown();

  std::cout << "Type something else to exit program." << std::endl;
  std::cin >> dummyBlock;

  return 0;
}
