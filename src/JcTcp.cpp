#include "JcTcp.hpp"

int jc_socket(JC::TcpSocket& sock,
              const JC::SocketType socket_type,
              const int port,
              const std::string& server_ip = "") {
  sock.open(socket_type, port, server_ip);
  return 0;
}

