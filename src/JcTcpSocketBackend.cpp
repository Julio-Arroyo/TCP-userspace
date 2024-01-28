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
      bool ready_to_close{false};
      {
        std::lock_guard<std::mutex> close_lock_guard(closeMutex);
        ready_to_close = dying;
      }

      size_t num_unsent_bytes;
      {  // Frontend updates nextToWrite... so critical section
        std::lock_guard<std::mutex> write_lock_guard{writeMutex};
        num_unsent_bytes = sendInfo.nextToWrite - sendInfo.nextToSend;
      }

      if (num_unsent_bytes > 0) {
        TcpSocket::sendNewData(num_unsent_bytes);
      } else if (ready_to_close) {
        break;
      }
      TcpSocket::resendOldData();  // retransmit unACK'd data

      // Check if there is any data to receive
      TcpSocket::receiveIncomingData(ReadMode::NO_WAIT);

      /*
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
      //
      // Check if there is any data to receive
      TcpSocket::receiveIncomingData(ReadMode::NO_WAIT);
      */

    }
  }

  void TcpSocket::sendNewData(size_t num_unsent_bytes) {
    size_t nbytes_unacked = sendInfo.nextToSend - sendInfo.lastAck;
    size_t send_capacity = std::max(0, static_cast<int>(sendInfo.advertisedWindow - nbytes_unacked));
    size_t nbytes_to_send = std::min(send_capacity, num_unsent_bytes);
    size_t nbytes_sent{0};
    std::chrono::time_point<CLOCK> transmission_time = CLOCK::now();

    // split nbytes_to_send into packets
    while (nbytes_sent < nbytes_to_send) {
      size_t remaining_bytes = nbytes_to_send - nbytes_sent;
      size_t payload_size = std::min(remaining_bytes, MAX_PAYLOAD_SIZE);

      // Prepare packet (header and payload)
      size_t packet_len = sizeof(JC::TcpHeader) + payload_size;
      uint32_t seq_num = sendInfo.nextToSend + nbytes_sent;
      std::vector<uint8_t> packet(packet_len);
      initHeader(static_cast<JC::TcpHeader*>(packet.data()),
                 myPort,
                 ntohs(conn.sin_port),
                 seq_num,
                 recvInfo.nextExpected,  // ackNum
                 sizeof(JC::TcpHeader),
                 packet_len,
                 JC_TCP_ACK_FLAG,
                 recvInfo.getAdvertisedWindow(), 
                 UNUSED);
      for (int i = 0; i < payload_size; i++) {
        size_t ofs = (sendInfo.nextToSend + nbytes_sent + i) % BUF_CAP;
        packet[sizeof(JC::TcpHeader) + i] = sendBuf[ofs];
      }

      sendto(udpSocket,
             static_cast<void*>(packet),
             packet_len,
             UNUSED,             // flags
             (sockaddr*) &conn,  // dest_addr
             sizeof(conn));
      nbytes_sent += payload_size;

      // record info in case packet is not ACK'd
      JC::RetransmissionInfo retransmission_info;
      retransmission_info.transmissionTime = transmission_time;
      retransmission_info.packet = std::move(packet);
      unackedPacketsInfo.push_back(retransmission_info);
    }
  }

  void resendOldData() {
    std::chrono::time_point<CLOCK> now = CLOCK::now();
    std::list<JC::RetransmissionInfo>::iterator it = unackedPacketsInfo.begin();
    while (it != unackedPacketsInfo.end()) {
      assert(it->packet.empty());
      void* packet = static_cast<void*>(it->packet.data());
      
      // It may have already been ACK'd
      JC::TcpHeader* hdr = static_cast<JC::TcpHeader*>(packet);
      if (hdr->seqNum < sendInfo.lastAck) {
        unackedPacketsInfo.pop_front();
        continue;
      }

      // if this packet's timer hasn't expired, subsequent packets' haven't either
      std::chrono::milliseconds elapsed_time
        = std::chrono::duration_cast<std::chrono::milliseconds>(now - retransmission_info.transmissionTime);
      if (elapsed_time.count() < ACK_TIMEOUT) {
        break;
      }

      // Resend packet
      hdr->ackNum = recvInfo.nextExpected;
      hdr->advertisedWindow = recvInfo.getAdvertisedWindow();
      sendto(udpSocket,
             packet,
             packet.size(),
             UNUSED,         // flags
             (sockaddr*) &conn,
             sizeof(conn));
    }
  }

  /*
  void TcpSocket::sendOnePacketAtATime(std::vector<uint8_t>& dataToSend) {
    size_t len = dataToSend.size();
    size_t bytes_sent{0};

    while (bytes_sent < len) {
      size_t remaining_bytes = len - bytes_sent;
      size_t payload_size = std::min(remaining_bytes, MAX_PAYLOAD_SIZE);

      // *** prepare packet (header and payload) ***
      size_t packet_len = sizeof(JC::TcpHeader) + payload_size;
      std::vector<uint8_t> packet(packet_len);
      void* hdr = packet.data() + bytes_sent;  // TODO: i think it is wrong to add bytes_sent
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
  */

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

    // TODO: put all this into a method that returns pointer to packet
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

      bool arrived_in_order = recvdHdr.seqNum == recvState.nextExpected;
      size_t payload_len = recvdHdr.packetLen - recvdHdr.headerLen;
      uint32_t last_seq_num = recvdHdr.seqNum + payload_len - 1;
      if (last_seq_num - recvState.nextToRead < BUF_CAP) {
        {
          // save payload into recvBuf
          std::lock_guard<std::mutex> received_lock_guard{receivedMutex};
          for (int i = 0; i < payload_len; i++) {
            size_t idx = (recvHdr.seqNum + i) % BUF_CAP;
            recvBuf[idx] = recvdPacket[recvdHdr.headerLen + i];
            
            if (!arrived_in_order) {
              yetToAck[idx] = true;  // mark out-of-order bytes
            }
          }
          recvState.lastReceived = std::max(recvState.lastReceived,
                                            last_seq_num);

          /* nextExpected must be updated when packets arrive in order.
             NOTE: frontend uses nextExpected to know last bytes
                   available to read, it must be updated in critical section */
          if (arrived_in_order) {
            recvState.nextExpected = recvdHdr.seqNum + payload_len;
            while (yetToAck[recvState.nextExpected % BUF_CAP]) {
              yetToAck[(recvState.nextExpected++) % BUF_CAP] = false;
            }
            receivedCondVar.notify_all();  // notify TCP's frontend: new data to read
          }
        }

        if (arrived_in_order) {  // must send ACK
          assert((recvdHdr.flags & JC_TCP_ACK_FLAG) == 0);
          size_t adv_window = BUF_CAP - (recvState.lastReceived + 1 - recvState.nextToRead);
          JC::TcpHeader ackHdr{JC_TCP_IDENTIFIER,
                               myPort,                         // srcPort
                               ntohs(conn.sin_port),           // destPort
                               UNUSED,                         // seqNum
                               recvState.nextExpected,         // ackNum
                               sizeof(JC::TcpHeader),          // headerLen
                               sizeof(JC::TcpHeader),          // packetLen
                               JC_TCP_ACK_FLAG,
                               adv_window,                     // advertisedWindow
                               UNUSED};                        // extensionLen

          /* NOTE: sendto blocks when it's send buffer is full, 
                   unless in non-blocking I/O mode.
                   So, that's why it's outside critical section */
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
}

