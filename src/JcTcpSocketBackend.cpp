#include "JcTcpSocket.hpp"

namespace JC {
  /**
   * Infinitely loop until connection is torn down.
   *   - Tear down if 'dying' has been set and there is no 
   *     pending data to send.
   *   - Whenever there is data in 'sending_buf', empty it and put the data
   *     in individual packets and send them over the socket one at a time.
   *   - RECEIVING END: check if there is incoming data, which may be an ACK or an actual packet
   *     with payload. If it's the former, update sendState. If it is the latter
   *     save the received the 
   */
  void TcpSocket::beginBackend() {
    for (;;) {
      // Check if App called close() already
      bool readyToClose{false};
      {
        std::lock_guard<std::mutex> close_lock_guard(closeMutex);
        readyToClose = dying;
      }

      // Transmit any data in sending_buf
      std::vector<uint8_t> dataToSend;
      {
        std::lock_guard<std::mutex> write_lock_guard{writeMutex};

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

      // NOTE: data is sent outside writeMutex critical section
      //       so that App can call write() and put data in sending_buf
      if (!dataToSend.empty()) {
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
      size_t payload_size = std::min(remaining_bytes, MAX_PAYLOAD_SIZE);

      // *** prepare packet (header and payload) ***
      size_t packet_len = sizeof(JC::TcpHeader) + payload_size;
      std::vector<uint8_t> packet(packet_len);
      void* hdr = packet.data() + bytes_sent;  // TODO: i think adding bytes_sent is a bug
      initHeader(static_cast<JC::TcpHeader*>(hdr),
                 myPort,                  // srcPort
                 ntohs(conn.sin_port),    // destPort
                 sendState.lastAck,       // seqNum
                 recvState.nextExpected,  // ackNum
                 sizeof(JC::TcpHeader),   // header_len
                 packet_len,
                 JC_TCP_ACK_FLAG,         // flags   // TODO should have ack flag: every segment reports what seqnum the sender expects next
                 1,                       // advertised_window
                 UNUSED);                 // extensionLen;
      // prepare payload
      std::copy(dataToSend.begin() + bytes_sent,
                dataToSend.begin() + bytes_sent + payload_size,
                packet.data() + bytes_sent + sizeof(JC::TcpHeader));

      for (;;) {
        // send one packet (i.e send 'packet_len' bytes starting at 'hdr')
        sendto(udpSocket,          // sockfd
               hdr,                // buf
               packet_len,         // len
               0,                  // flags
               (sockaddr*) &conn,  // dest_addr
               sizeof(conn));      // addrlen
        sendState.lastSent += payload_size;
 
        // wait for ACK
        TcpSocket::receiveIncomingData(ReadMode::TIMEOUT);

        if (sendState.lastAck == sendState.lastSent) {
          // packet has been ACKed, send next 
          break;
        } else {
          sendState.lastSent -= payload_size;
        }
      }

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
                               (void*) &recvdHdr,       /* buf */
                               sizeof(JC::TcpHeader),   /* len */
                               flags,
                               (sockaddr*) &conn,       /* src_addr */
                               &conn_len);              /* addrlen */

    if (minBytesAvl >= static_cast<int>(sizeof(JC::TcpHeader))) {  // >= 1 pkt worth of data
      // extract the entire packet (header + payload) from socket
      size_t nBytesRecvd{0};
      std::vector<uint8_t> recvdPacket(recvdHdr.packetLen);

      // loop until all 'packet_len' bytes are extracted from the socket
      while (nBytesRecvd < recvdHdr.packetLen) {
        nBytesRecvd += recvfrom(udpSocket,
                                static_cast<void*>(recvdPacket.data() + nBytesRecvd),
                                recvdHdr.packetLen - nBytesRecvd,
                                UNUSED,  // flags
                                (sockaddr*) &conn,
                                &conn_len);
      }
      assert(recvdPacket.size() == recvdHdr.packetLen);
      assert(recvdHdr.identifier == JC_TCP_IDENTIFIER);

      if (recvdHdr.flags & JC_TCP_ACK_FLAG) {
        if (sendState.lastAck > recvdHdr.ackNum) {
          std::cerr << "ERROR receiveIncomingData:" << std::endl;
          std::cerr << "lastAck=" << sendState.lastAck
                    << " > "
                    << recvdHdr.ackNum << "=recvdAckNum" << std::endl;
          assert(false);
          return;
        }

        sendState.lastAck = recvdHdr.ackNum;
      }

      if (recvdHdr.seqNum == recvState.nextExpected) {
        size_t payload_len = recvdHdr.packetLen - recvdHdr.headerLen;
        recvState.nextExpected = recvdHdr.seqNum + payload_len;

        // Write the received payload into receivedBuf
        {
          std::lock_guard<std::mutex> received_lock_guard{receivedMutex};
          size_t curr_recvd_size = receivedBuf.size();
          receivedBuf.resize(curr_recvd_size + payload_len);
          std::copy(recvdPacket.begin() + recvdHdr.headerLen,  // start at payload
                    recvdPacket.end(),
                    receivedBuf.begin() + curr_recvd_size);  // after any data already in buffer
        }
        receivedCondVar.notify_all();  // signal App interface there is data available


        // REPLY with ACK
        JC::TcpHeader ackHeader;
        initHeader(&ackHeader,
                   myPort,                         // srcPort
                   ntohs(conn.sin_port),           // destPort
                   UNUSED,                         // seqNum
                   recvState.nextExpected,         // ackNum
                   sizeof(JC::TcpHeader),          // headerLen
                   sizeof(JC::TcpHeader),          // packetLen
                   JC_TCP_ACK_FLAG,
                   UNUSED,                         // advertisedWindow
                   UNUSED);                        // extensionLen
        sendto(udpSocket,         // sockfd
               static_cast<void*>(&ackHeader), // buf
               sizeof(JC::TcpHeader),        // len
               0,                 // flags
               (sockaddr*) &conn,  // dest_addr
               sizeof(conn));     // addrlen
      }

    }
  }
}

