#include "JcTcpSocket.hpp"

namespace JC {
  /**
   * Infinitely loop until connection is torn down.
   *   - Tear down if 'dying' has been set and there is no 
   *     pending data to send.
   *   - Whenever there is data in 'sending_buf', empty it and put the data
   *     in individual packets and send them over the network one at a time.
   *   - Constantly check if there is incoming data, if there is TODO
   */
  void TcpSocket::beginBackend() {
    for (;;) {
      // Check if App called close() already
      bool readyToClose{false};
      {
        std::lock_guard<std::mutex> close_lock_guard{close_mutex};
        readyToClose = dying;
      }

      // Transmit any data in sending_buf
      std::vector<uint8_t> dataToSend;
      {
        std::lock_guard<std::mutex> write_lock_guard{write_mutex};

        size_t nBytesToSend = sendingBuf.size();
        if (nBytesToSend == 0) {
          if (readyToClose) {
            break;
          }
        } else {
          dataToSend = std::move(sendingBuf);
          assert(sendingBuf.size() == 0);
        }
      }
      // NOTE: data is sent outside write_mutex critical section
      //       so that App can call write() and put data in sending_buf
      if (dataToSend.empty()) {
        TcpSocket::sendOnePacketAtATime(dataToSend);  
      }

      // Check if there is any data to receive
      TcpSocket::receiveIncomingData(ReadMode::NO_WAIT);
    }
  }

  void TcpSocket::sendOnePacketAtATime(std::vector<uint8_t>& dataToSend) {
    size_t len = dataToSend.size();
    size_t bytes_sent{0};

    while (bytes_sent < len) {
      size_t remaining_bytes = len - bytes_sent;
      size_t payload_size = std::min(remaining_bytes, MaxPayloadSize);

      // prepare packet (header and payload)
      size_t packet_len = sizeof(JC::TcpHeader) + payload_size;
      void* packet = new uint8_t[packet_len];  // TODO use array?
      JC::TcpHeader* hdr = (JC::TcpHeader*) packet;
      initHeader(hdr,
                 my_port,                // srcPort
                 ntohs(conn->sin_port),  // destPort
                 sendState.lastAck,      // seqNum
                 UNUSED,                 // ackNum
                 sizeof(JC::TcpHeader),  // header_len
                 packet_len,
                 UNUSED,                 // TODO flags
                 1,                      // advertised_window
                 UNUSED);                // extensionLen;
      std::copy(dataToSend.begin() + bytes_sent,
                dataToSend.begin() + bytes_sent + payload_size,
                packet + sizeof(JC::TcpHeader) + bytes_sent);

      for (;;) {
        // send one packet
        sendto(udpSocket,         // sockfd
               (void*) packet,    // buf
               packet_len,        // len
               0,                 // flags
               (sockaddr*) conn,  // dest_addr
               sizeof(conn));     // addrlen
 
        // wait for ACK
        TcpSocket::receiveIncomingData(ReadMode::TIMEOUT);

        if (sendState.lastAck == sendState.lastSent) {
          // packet has been acked, send next 
          break;
        }
      }

      delete[] packet;
      bytes_sent += payload_size;
    }
  }

  /**
   * First extract packet. Then payload
   *
   * Case TIMEOUT:
   *   - Used when sending one packet, wait for receipt of ACK
   */
  void TcpSocket::receiveIncomingData(const JC::ReadMode readMode) {
    if (readMode == JC::ReadMode::BLOCK) {
      std::cerr << "Backend should not block indefinitely to receive data." << std::endl;
      return;
    }

    int minBytesAvl = 0;

    // optionally wait some time in case no data has been received yet
    if (readMode == JC::ReadMode::TIMEOUT) {
        // wait to see if data comes in
        struct pollfd pfd;
        pfd.fd = udpSocket;
        pfd.events = POLLIN;  // there is data to read
        poll(&pfd, 1 /* num pollfd's */, ACK_TIMEOUT /* ms */);
    } else {
      assert(readMode == JC::ReadMode::NO_WAIT);
    }

    // peek to see if there's at least a header's worth of data
    JC::TcpHeader recvdHdr;
    socklen_t conn_len = sizeof(conn);
    int flags = MSG_DONTWAIT |  /* make nonblocking call */
                MSG_PEEK;       /* next recvfrom() call returns same data) */
    int minBytesAvl = recvfrom(udpSocket,
                               (void*) &recvdHdr,   /* buf */
                               sizeof(JC::TcpHeader),  /* len */
                               flags,
                               (sockaddr*) conn,       /* src_addr */
                               &conn_len);             /* addrlen */

    if (minBytesAvl >= sizeof(JC::TcpHeader)) {  // >= 1 pkt worth of data
      // extract the entire packet (header + payload) from socket
      size_t nBytesRecvd{0};
      std::vector<uint8_t> recvdPacket(recvdHdr.packetLen);

      // loop until all 'packet_len' bytes are extracted from the socket
      while (nBytesRecvd < recvdHdr.packetLen) {
        nBytesRecvd += recvfrom(udpSocket,
                                static_cast<void*>(recvdPacket.data() + nBytesRecvd),
                                payload_len - nBytesRecvd,
                                UNUSED,  // flags
                                (sockaddr*) conn,
                                &conn_len);
      }
      assert(recvdPacket.size() == recvdHdr.packetLen);

      if (recvdHdr.flags & JC_TCP_ACK_FLAG) {
        assert(sendState.ackNum <= recvdHdr.ackNum);
        assert(nBytesRecvd == sizeof(JC::TcpHeader));

        sendState.lastAck = recvdHdr.ackNum;
        return;
      }

      std::unique_lock<std::mutex> cv_lock{read_mutex};  // needed by condvar
      cv_lock.lock();

      size_t curr_recvd_size = received_buf.size();
      received_buf.resize(curr_recvd_size + nBytesRecvd);
      std::copy(recvdPacket.begin(),
                recvdPacket.end(),
                received_buf.begin() + curr_recvd_size);
      wait_cond.notify_all();

      cv_lock.unlock();
    }
  }
}

