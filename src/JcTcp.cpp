#include "JcTcp.hpp"

int jc_socket(JC::TcpSocket& sock,
              const JC::SocketType socket_type,
              const int port,
              const std::string& server_ip) {
  return sock.open(socket_type, port, server_ip);
}

int jc_close(JC::TcpSocket& sock) {
  return sock.teardown();
}

int jc_read(JC::TcpSocket& sock,
            void* dest_buf,
            const int len, 
            const JC::ReadMode read_mode) {
  return sock.read(dest_buf, len, read_mode);
}

int jc_write(JC::TcpSocket& sock, void* src_buf, const int len) {
  return sock.write(src_buf, len);
}

