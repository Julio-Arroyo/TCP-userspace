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
      return EXIT_FAILURE;
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
          return EXIT_FAILURE;
        }
        conn = conn_;
        break;
      }
      case JC::SocketType::TCP_INITIATOR: {
        // bind to (INADDR_ANY, random usable port)
        // connect to (server_ip, port)
        if (server_ip == "") {
          throw std::invalid_argument("Cannot initiate connection with empty address '""'");
          return EXIT_FAILURE;
        }

        conn_.sin_family = AF_INET;
        conn_.sin_port = htons((uint16_t) port);
        conn_.sin_addr.saddr = inet_addr(server_ip.c_str());

        my_addr.sin_family = AF_INET;
        my_addr.sin_port = 0;
        my_addr.sin_addr.saddr = htonl(INADDR_ANY);
        if (-1 == bind(udpSocket, (sockaddr*) &my_addr, sizeof(my_addr))) {
          assert(false);
          return EXIT_FAILURE;
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

    return EXIT_SUCCESS;
  }

  int TcpSocket::read(void* dest_buf, const int len, const JC::ReadMode read_mode) {
    return 0;
  }

  int TcpSocket::write(void* src_buf, const int write_len) {
    if (write_len < 0) {
      std::cerr << "Cannot write '" << write_len << "' bytes. 'write_len' must must be non-negative." << std::endl; 
      return EXIT_FAILURE;
    }

    std::lock_guard<std::mutex> write_lock_guard{write_mutex};

    // copy data in 'src_buf' into 'sending_buf'
    int curr_sending_size = sending_buf.size();
    sending_buf.resize(curr_sending_size + write_len);  // ensure enough space for new 'write_len' bytes
    uint8_t* src_buf_bytes = static_cast<uint8_t*>(src_buf);
    std::copy(src_buf_bytes,
              src_buf_bytes + write_len,
              sending_buf.begin() + curr_sending_size);

    return EXIT_SUCCESS;
  }

  int TcpSocket::close() {
    return EXIT_FAILURE;
  }
}

