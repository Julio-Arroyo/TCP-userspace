#include "JcTcpSocket.hpp"

namespace JC {
  TcpSocket::TcpSocket() {

  }

  void TcpSocket::open(const JC::SocketType socket_type,
                  const int port,
                  const std::string& server_ip) {
    sockaddr_in conn_, my_addr;
    std::memset(&conn_, 0, sizeof(sockaddr_in));
    std::memset(&my_addr, 0, sizeof(sockaddr_in));
    my_port = port;
    type = socket_type;

    // create a UDP socket
    int sockfd = socket(AF_INET,    // domain
                        SOCK_DGRAM, // type
                        0);         // protocol
    if (sockfd == -1) {
      assert(false);
      return;
    }
    udpSocket = sockfd;

    switch (socket_type) {
      case JC::SocketType::TCP_LISTENER: {
        // bind to (INADDR_ANY, port)
        conn_.sin_family = AF_INET;
        conn_.sin_port = htons((uint16_t) port);
        conn_.sin_addr.saddr = htonl(INADDR_ANY);

        // allow same address to be reused promptly after connection teardown
        int optval = 1;
        setsockopt(udpSocket,
                   SO_SOCKET,              // manipulate options at socket API level
                   SO_REUSEADDR,           // name of option
                   (const void*) &optval,  // non-zero ==> enable option
                   sizeof(optval));
        if (-1 == bind(udpSocket, (sockaddr*) &conn_, sizeof(conn_))) {
          assert(false);
          return;
        }
        conn = conn_;
        break;
      }
      case JC::SocketType::TCP_INITIATOR: {
        // bind to (INADDR_ANY, random usable port)
        // connect to (server_ip, port)
        if (server_ip == "") {
          assert(false);  // TODO: raise error msg CANNOT CONNECT TO ""
          return;
        }

        conn_.sin_family = AF_INET;
        conn_.sin_port = htons((uint16_t) port);
        conn_.sin_addr.saddr = inet_addr(server_ip.c_str());

        my_addr.sin_family = AF_INET;
        my_addr.sin_port = 0;
        my_addr.sin_addr.saddr = htonl(INADDR_ANY);
        if (-1 == bind(udpSocket, (sockaddr*) &my_addr, sizeof(my_addr))) {
          assert(false);
          return;
        }
        conn = conn_;
        break;                                       
      }
    }
    socklen_t my_addr_sz = sizeof(my_addr);
    getsockname(udpSocket, (sockaddr*) &my_addr, &my_addr_sz);
    my_port = ntohs(my_addr.sin_port);

    received_buf = NULL;
    received_len = 0;

    sending_buf = NULL;
    sending_len = 0;

    dying = 0;
    window.next_seq_expected = 0;
    window.last_ack_received = 0;

    backendThread = std::thread(begin_backend, this);
  }

  int TcpSocket::read() {}

  int TcpSocket::write() {}

  int TcpSocket::close() {}
}

