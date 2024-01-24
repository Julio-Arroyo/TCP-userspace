#include "JcTcpSocket.hpp"

namespace JC {
  TcpSocket::TcpSocket() {
    std::srand(JC_TCP_IDENTIFIER);
  }

  int TcpSocket::open(const JC::SocketType socket_type,
                  const int port,
                  const std::string& server_ip) {
    sockaddr_in conn_, my_addr;
    std::memset(&conn_, 0, sizeof(sockaddr_in));
    std::memset(&my_addr, 0, sizeof(sockaddr_in));
    type = socket_type;

    // create a UDP socket
    int sockfd = socket(AF_INET,    // domain
                        SOCK_DGRAM, // type
                        0);         // protocol
    if (sockfd == -1) {
      assert(false);
      return  JC_EXIT_FAILURE;
    }
    udpSocket = sockfd;

    switch (socket_type) {
      case JC::SocketType::TCP_LISTENER: {
        // bind to (INADDR_ANY, port)
        conn_.sin_family = AF_INET;
        conn_.sin_port = htons((uint16_t) port);
        conn_.sin_addr.s_addr = htonl(INADDR_ANY);

        // allow same address to be reused promptly after connection teardown
        int optval = 1;
        setsockopt(udpSocket,
                   SOL_SOCKET,             // manipulate options at socket API level
                   SO_REUSEADDR,           // name of option
                   (const void*) &optval,  // non-zero ==> enable option
                   sizeof(optval));
        if (-1 == bind(udpSocket, (sockaddr*) &conn_, sizeof(conn_))) {
          assert(false);
          return JC_EXIT_FAILURE;
        }
        conn = conn_;
        myPort = port;
        
        // *** Three-way handshake ***

        // wait for client's initiation request
        socklen_t conn_len = sizeof(conn);
        JC::TcpHeader initiationRequest;
        while (true) {
          struct pollfd pfd;
          pfd.fd = udpSocket;
          pfd.events = POLLIN;  // there is data to read
          std::cout << "Waiting for client to initiate connection" << std::endl;
          if (0 != poll(&pfd, 1, 10 * 1000)) {
            int bytes_recvd = recvfrom(udpsocket,
                                       (void*) &initiationRequest,
                                       sizeof(JC::TcpHeader),
                                       MSG_DONTWAIT,
                                       static_cast<sockaddr*>(&conn),
                                       &conn_len);
            if (bytes_recvd == sizeof(JC::TcpHeader) &&
                (JC_TCP_SYN_FLAG & initiationRequest.flags)) {
              std::cout << "Received client connection request" << std::endl;
              break;
            }
          }
        }

        // Respond to connection request, defining starting seqNum
        uint32_t first_seq_num = std::rand();
        std::cout << "Server start seqnum " << first_seq_num << std::endl;  // DEBUG
        JC::TcpHeader initiationResponse;
        initHeader(&initiationResponse,
                   myPort,
                   ntohs(conn.sin_port),
                   first_seq_num,
                   initiationRequest.seqNum + 1,  // ackNum
                   sizeof(JC::TcpHeader),
                   sizeof(JC::TcpHeader),
                   JC_TCP_SYN_FLAG | JC_TCP_ACK_FLAG,
                   UNUSED,
                   UNUSED);
        sendto(udpSocket,
               static_cast<void*>(&initiationResponse),
               sizeof(JC::TcpHeader),
               0,
               static_cast<sockaddr*>(&conn),
               sizeof(conn));
        std::cout << "Accepted connection. Waiting for client confirmation"
                  << std::endl;

        // wait for Ack
        JC::TcpHeader confirmationAck;
        int bytes_confi = recvfrom(udpsocket,
                                   (void*) &initiationRequest,
                                   sizeof(JC::TcpHeader),
                                   MSG_DONTWAIT,
                                   static_cast<sockaddr*>(&conn),
                                   &conn_len);
        if (bytes_confi == sizeof(JC::TcpHeader) &&
            (confirmationAck.flags & JC_TCP_ACK_FLAG) &&
            (confirmationAck.ackNum == first_seq_num + 1)) {
          std::cout << "Three-way handshake successfully completed."
                    << std::endl;
        }
        break;
      }
      case JC::SocketType::TCP_INITIATOR: {
        // bind to (INADDR_ANY, random usable port)
        // connect to (server_ip, port)

        if (server_ip == "") {
          throw std::invalid_argument("Cannot initiate connection with empty address '""'");
          return JC_EXIT_FAILURE;
        }

        conn_.sin_family = AF_INET;
        conn_.sin_port = htons((uint16_t) port);
        conn_.sin_addr.s_addr = inet_addr(server_ip.c_str());

        my_addr.sin_family = AF_INET;
        my_addr.sin_port = 0;
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (-1 == bind(udpSocket, (sockaddr*) &my_addr, sizeof(my_addr))) {
          assert(false);
          return JC_EXIT_FAILURE;
        }
        conn = conn_;

        socklen_t my_addr_sz = sizeof(my_addr);
        getsockname(udpSocket, (sockaddr*) &my_addr, &my_addr_sz);
        myPort = ntohs(my_addr.sin_port);

        // Request to initiate connection
        uint32_t first_seq_num = std::rand();
        std::cout << "Server start seqnum " << first_seq_num << std::endl;
        JC::TcpHeader connectionRequest;
        initHeader(&connectionRequest,
                   myPort,
                   ntohs(conn.sin_port),
                   first_seq_num,
                   UNUSED,  // ackNum
                   sizeof(JC::TcpHeader),
                   sizeof(JC::TcpHeader),
                   JC_TCP_SYN_FLAG,
                   UNUSED,
                   UNUSED);
        sendto(udpSocket,
               static_cast<void*>(&connectionRequest),
               sizeof(JC::TcpHeader),
               0,
               static_cast<sockaddr*>(&conn),
               sizeof(conn));
        std::cout << "Waiting for server to accept connection request..."
                  << std::endl;
        // TODO
        // receiveblocking
        // send
        break;                                       
      }
    }

    assert(myPort != 0);
    backendThread = std::thread(&TcpSocket::beginBackend, this);

    return JC_EXIT_SUCCESS;
  }

  int TcpSocket::read(void* dest_buf,
                      const int len,
                      const JC::ReadMode read_mode) {
    if (read_mode == JC::ReadMode::TIMEOUT) {
      std::cerr << "jc_read does not implement read_mode=TIMEOUT" << std::endl;
      return JC_EXIT_FAILURE;
    } else if (len < 0) {
      std::cerr << "Cannot read negative no. of bytes" << std::endl;
      return JC_EXIT_FAILURE;
    }

    std::unique_lock<std::mutex> read_unique_lock(receivedMutex);  // acquire lock

    if (read_mode == JC::ReadMode::BLOCK) {
      while (receivedBuf.empty()) {
        receivedCondVar.wait(read_unique_lock);
      }
    }

    size_t read_len = std::min(static_cast<size_t>(len),
                               receivedBuf.size());
    std::copy(receivedBuf.begin(),
              receivedBuf.begin() + read_len,
              static_cast<uint8_t*>(dest_buf));
    receivedBuf.erase(receivedBuf.begin(),
                      receivedBuf.begin() + read_len);

    read_unique_lock.unlock();

    return read_len;
  }

  /**
   * Writes data to the JC-TCP socket.
   *
   * From an implementation perspective, the data in src_buf is copied
   * into sendingBuf. The Backend is then responsible for emptying it
   * and actually sending the data over the network.
   */
  int TcpSocket::write(void* src_buf, const int write_len) {
    if (write_len < 0) {
      std::cerr << "Cannot write '" << write_len << "' bytes. 'write_len' must must be non-negative." << std::endl; 
      return JC_EXIT_FAILURE;
    }

    std::lock_guard<std::mutex> write_lock_guard{writeMutex};

    // copy data from 'src_buf' into 'sendingBuf'
    int curr_sending_size = sendingBuf.size();
    sendingBuf.resize(curr_sending_size + write_len);  // add space for new 'write_len' bytes
    uint8_t* src_buf_bytes = static_cast<uint8_t*>(src_buf);
    std::copy(src_buf_bytes,
              src_buf_bytes + write_len,
              sendingBuf.begin() + curr_sending_size);

    return JC_EXIT_SUCCESS;
  }

  int TcpSocket::teardown() {
    {
      std::lock_guard<std::mutex> close_lock_guard{closeMutex};
      dying = true;
    }

    backendThread.join();

    return close(udpSocket);
  }
}

