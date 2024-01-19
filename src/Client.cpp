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
  std::cout << "Type a message to send to the Server: " << std::endl;
  std::string msg;
  std::cin >> msg;
  sock.write(static_cast<void*>(msg.data()), msg.length());

  sock.teardown();

  return 0;
}
