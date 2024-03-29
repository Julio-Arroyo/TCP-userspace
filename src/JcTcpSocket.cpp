#include "JcTcpSocket.hpp"

namespace JC {
  TcpSocket::TcpSocket() {
    // std::srand(JC_TCP_IDENTIFIER);
  }

  int TcpSocket::open(const JC::SocketType socket_type,
                      const int port,
                      const std::string& server_ip) {
    sockaddr_in conn_, my_addr;
    std::memset(&conn_, 0, sizeof(sockaddr_in));
    std::memset(&my_addr, 0, sizeof(sockaddr_in));
    type = socket_type;

    // create a UDP socket
    int sockfd = socket(AF_INET,     /* domain */
                        SOCK_DGRAM,  /* type */
                        0);          /* protocol */
    if (sockfd == -1) {
      ERROR("Could not create UDP socket.");
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
          ERROR("Listener JC-TCP socket could not bind underlying udpSocket");
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
          LOG("Waiting for client to initiate connection...");
          if (0 != poll(&pfd, 1, 10*1000 /* TIMEOUT */)) {
            int bytes_recvd = recvfrom(udpSocket,
                                       (void*) &initiationRequest,
                                       sizeof(JC::TcpHeader),
                                       MSG_DONTWAIT,
                                       (sockaddr*) &conn,
                                       &conn_len);
            if (bytes_recvd == sizeof(JC::TcpHeader) &&
                (JC_TCP_SYN_FLAG & initiationRequest.flags)) {
              recvInfo.nextExpected = initiationRequest.seqNum + 1;
              recvInfo.nextToRead = initiationRequest.seqNum + 1;
              recvInfo.lastReceived = initiationRequest.seqNum;
              sendInfo.otherSideAdvWindow = initiationRequest.advertisedWindow;
 
              // assert(sendInfo.otherSideAdvWindow == BUF_CAP);  // SWP
              assert(sendInfo.otherSideAdvWindow == FIXED_WINDOW);

              LOG("Received client connection request...");
              break;
            }
          }
        }

        // Respond to connection request, defining starting seqNum
        uint32_t first_seq_num = std::rand();
        JC::TcpHeader initiationResponse;
        initHeader(&initiationResponse,
                   myPort,
                   ntohs(conn.sin_port),
                   first_seq_num,
                   recvInfo.nextExpected,  // ackNum
                   sizeof(JC::TcpHeader),
                   sizeof(JC::TcpHeader),
                   JC_TCP_SYN_FLAG | JC_TCP_ACK_FLAG,
                   recvInfo.getAdvertisedWindow(),   // advertisedWindow
                   UNUSED);  // extensionLen
        sendto(udpSocket,
               static_cast<void*>(&initiationResponse),
               sizeof(JC::TcpHeader),
               0,
               (sockaddr*) (&conn),
               sizeof(conn));
        sendInfo.nextToSend = first_seq_num + 1;
        sendInfo.nextToWrite = first_seq_num + 1;
        break;
      }
      case JC::SocketType::TCP_INITIATOR: {
        if (server_ip == "") {
          throw std::invalid_argument("Cannot initiate connection with empty address '""'");
          return JC_EXIT_FAILURE;
        }

        // connect to (server_ip, port)
        conn_.sin_family = AF_INET;
        conn_.sin_port = htons((uint16_t) port);
        conn_.sin_addr.s_addr = inet_addr(server_ip.c_str());

        // bind to (INADDR_ANY, random usable port)
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = 0;
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (-1 == bind(udpSocket, (sockaddr*) &my_addr, sizeof(my_addr))) {
          ERROR("Initiator JC-TCP socket could not bind underlying udpSocket");
          return JC_EXIT_FAILURE;
        }
        conn = conn_;

        socklen_t my_addr_sz = sizeof(my_addr);
        getsockname(udpSocket, (sockaddr*) &my_addr, &my_addr_sz);
        myPort = ntohs(my_addr.sin_port);

        // *** Begin three-way handshake ***
        uint32_t first_seq_num = std::rand();
        JC::TcpHeader connectionRequest;
        initHeader(&connectionRequest,
                   myPort,
                   ntohs(conn.sin_port),
                   first_seq_num,
                   UNUSED,  // ackNum
                   sizeof(JC::TcpHeader),
                   sizeof(JC::TcpHeader),
                   JC_TCP_SYN_FLAG,
                   FIXED_WINDOW,  // SWP: BUF_CAP,   // advertisedWindow
                   UNUSED);  // extensionLen
        // Repeatedly request to initiate connection, until server accepts
        for (;;) {
          sendto(udpSocket,
                 static_cast<void*>(&connectionRequest),
                 sizeof(JC::TcpHeader),
                 0,
                 (sockaddr*) &conn,
                 sizeof(conn));
          sendInfo.nextToSend = first_seq_num + 1;
          sendInfo.nextToWrite = first_seq_num + 1;
          LOG("Waiting for server to accept connection request...");

          struct pollfd pfd;
          pfd.fd = udpSocket;
          pfd.events = POLLIN;
          if (0 != poll(&pfd, 1, ACK_TIMEOUT)) {
            // receive server's connection acceptance
            JC::TcpHeader server_response;
            socklen_t conn_len = sizeof(conn);
            int nbytes_recvd = recvfrom(udpSocket,
                                        (void*) &server_response,
                                        sizeof(JC::TcpHeader),     // len
                                        MSG_DONTWAIT,
                                        (sockaddr*)(&conn),
                                        &conn_len);

            if ((nbytes_recvd == static_cast<int>(sizeof(JC::TcpHeader))) &&
                (server_response.flags & JC_TCP_SYN_FLAG) &&
                (server_response.flags & JC_TCP_ACK_FLAG) &&
                (server_response.ackNum == first_seq_num + 1))
            {
              LOG("Server accepted connection!");
              sendInfo.lastAck = server_response.ackNum;
              recvInfo.nextExpected = server_response.seqNum + 1;
              recvInfo.nextToRead = server_response.seqNum + 1;
              recvInfo.lastReceived = server_response.seqNum;
              sendInfo.otherSideAdvWindow = server_response.advertisedWindow;

              // assert(sendInfo.otherSideAdvWindow == BUF_CAP);  // SWP
              assert(sendInfo.otherSideAdvWindow == FIXED_WINDOW);

              // send ACK of server's acceptance
              JC::TcpHeader acceptance_ack;
              initHeader(&acceptance_ack,
                         myPort,
                         ntohs(conn.sin_port),
                         UNUSED,  // seqNum: seq will be ignored because it's not expected 
                         recvInfo.nextExpected,   // ackNum
                         sizeof(JC::TcpHeader),
                         sizeof(JC::TcpHeader),
                         JC_TCP_ACK_FLAG,
                         recvInfo.getAdvertisedWindow(), 
                         UNUSED);                 // extensionLen
              sendto(udpSocket,
                     static_cast<void*>(&acceptance_ack),
                     sizeof(JC::TcpHeader),
                     0,
                     (sockaddr*) (&conn),
                     sizeof(conn));
              break;  // from infinite loop
            }
          }
        }
        break;  // from switch                            
      }
      default: {
        ERROR("Socket type must be either listener or initiator.");
        return JC_EXIT_FAILURE;
        break;
      }
    }

    assert(myPort != 0);
    assert(sendInfo.nextToSend == sendInfo.nextToWrite);
    assert(recvInfo.lastReceived + 1 == recvInfo.nextToRead);
    assert(recvInfo.nextToRead == recvInfo.nextExpected);

    LOG("Launching backend...");
    backendThread = std::thread(&TcpSocket::beginBackend, this);

    return JC_EXIT_SUCCESS;
  }

  int TcpSocket::read(void* dest_buf,
                      const int len,
                      const JC::ReadMode read_mode) {
    if (read_mode == JC::ReadMode::TIMEOUT) {
      ERROR("read() only supports 'read_mode' BLOCK or NO_WAIT.");
      return JC_EXIT_FAILURE;
    } else if (len < 0) {
      ERROR("read() can only read non-negative number of bytes");
      return JC_EXIT_FAILURE;
    }

    bool other_side_closed = false;
    if (type == JC::SocketType::TCP_LISTENER) {
      // INITIATOR may be done sending data
      std::lock_guard<std::mutex> close_lock_guard(closeMutex);
      other_side_closed = dying;
    }

    std::unique_lock<std::mutex> read_unique_lock(receivedMutex);  // acquire lock

    // Block until there is data to read
    size_t unread_bytes = recvInfo.nextExpected - recvInfo.nextToRead;
    if (unread_bytes == 0 && other_side_closed) {
      return 0;
    }

    if (read_mode == JC::ReadMode::BLOCK) {
      while (unread_bytes == 0 && !other_side_closed) {
        receivedCondVar.wait(read_unique_lock);
        unread_bytes = recvInfo.nextExpected - recvInfo.nextToRead;
        std::lock_guard<std::mutex> close_lock_guard(closeMutex);
        other_side_closed = dying;
      }
    }

    // move bytes from recvBuf into dest_buf
    size_t read_len = std::min(static_cast<size_t>(len), unread_bytes);
    uint8_t* dest_buf_bytes = static_cast<uint8_t*>(dest_buf);
    for (int i = 0; i < read_len; i++) {
      dest_buf_bytes[i] = recvBuf[(recvInfo.nextToRead + i) % BUF_CAP];
    }
    recvInfo.nextToRead += read_len;

    return read_len;
  }

  /**
   * Writes data to the JC-TCP socket.
   *
   * From an implementation perspective, the data in src_buf is copied
   * into sendBuf. The Backend is then responsible for emptying it
   * and actually sending the data over the network.
   */
  int TcpSocket::write(void* src_buf, const int write_len) {
    if (write_len < 0) {
      ERROR("write() cannot transmit a negative number of bytes.");
      return JC_EXIT_FAILURE;
    }

    std::lock_guard<std::mutex> write_lock_guard{writeMutex};

    uint8_t* src_buf_bytes = static_cast<uint8_t*>(src_buf);
    // copy data from 'src_buf' into 'sendBuf'
    for (uint32_t i = 0; i < write_len; i++) {
      sendBuf[(sendInfo.nextToWrite + i) % BUF_CAP] = src_buf_bytes[i];
    }
    sendInfo.nextToWrite += write_len;

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

  void TcpSocket::transmitData(void* buf, int nbytes,
                               uint32_t seq_num, uint32_t ack_num,
                               uint8_t tcp_flags, int sendto_flags) {
    uint16_t packet_len = sizeof(JC::TcpHeader) + nbytes;
    JC::TcpHeader hdr{JC_TCP_IDENTIFIER,
                      myPort,                          // srcPort
                      ntohs(conn.sin_port),            // destPort
                      seq_num,                         // seqNum
                      ack_num,                         // ackNum
                      sizeof(JC::TcpHeader),           // headerLen
                      packet_len,                      // packetLen
                      tcp_flags,
                      recvInfo.getAdvertisedWindow(),  // advertisedWindow
                      UNUSED};                         // extensionLen

    // TODO: add payload

    /* NOTE: sendto blocks when it's send buffer is full, 
             unless in non-blocking I/O mode.
             So, that's why it's outside critical section */
    sendto(udpSocket,         // sockfd
           static_cast<void*>(&hdr),  // buf
           packet_len,     // len
           0,                 // flags
           (sockaddr*) &conn,  // dest_addr
           sizeof(conn));     // addrlen
  }
}

