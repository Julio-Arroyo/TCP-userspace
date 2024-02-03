#include "JcTcpSocket.hpp"

namespace JC {
  /**
   * Infinitely loop until connection is torn down.
   *   - Tear down connection if 'dying' has been set and there is no 
   *     pending data to send.
   *   - SENDING SIDE: Whenever there is data in 'sendBuf', empty it and put the data
   *     in individual packets and send them over the socket one at a time.
   *   - RECEIVING SIDE:
   *       > check if there is incoming data, which may be an ACK or an actual packet
   *       > if received an ACK:
   *         + update recvInfo
   *         + re-estimate RTT (if corresponding packet was transmitted only once).
   *       > if received packet with data in payload:
   *         + save data in recvBuf if within advertised window.
   */
  void TcpSocket::beginBackend() {
    bool ready_to_close{false};
    size_t num_unsent_bytes;
    for (;;) {
      {  // Check if App called close() already
        std::lock_guard<std::mutex> close_lock_guard(closeMutex);
        ready_to_close = dying;
      }

      {  // Frontend updates nextToWrite, so it's a critical section
        std::lock_guard<std::mutex> write_lock_guard{writeMutex};
        num_unsent_bytes = sendInfo.nextToWrite - sendInfo.nextToSend;
      }

      if (num_unsent_bytes > 0) {
        TcpSocket::sendNewData(num_unsent_bytes);
      } else if (ready_to_close) {
        LOG("Closing backend...");
        break;
      }
      TcpSocket::resendOldData();  // retransmit unACK'd data

      // Check if there is any data to receive
      TcpSocket::receiveIncomingData(ReadMode::NO_WAIT);
    }
  }

  void TcpSocket::sendNewData(size_t num_unsent_bytes) {
    size_t nbytes_unacked = sendInfo.nextToSend - sendInfo.lastAck;
    size_t send_capacity = std::max(0, static_cast<int>(sendInfo.otherSideAdvWindow - nbytes_unacked));
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
      JC::TcpHeader* hdr = (JC::TcpHeader*) packet.data();
      initHeader(hdr,
                 myPort,
                 ntohs(conn.sin_port),
                 seq_num,
                 recvInfo.nextExpected,  // ackNum
                 sizeof(JC::TcpHeader),
                 packet_len,
                 JC_TCP_ACK_FLAG,
                 recvInfo.getAdvertisedWindow(), 
                 UNUSED);
      uint8_t* payload = packet.data() + sizeof(JC::TcpHeader);
      for (int i = 0; i < payload_size; i++) {
        size_t ofs = (sendInfo.nextToSend + nbytes_sent + i) % BUF_CAP;
        payload[i] = sendBuf[ofs];
      }

      // transmit packet
      sendto(udpSocket,
             static_cast<void*>(packet.data()),
             packet_len,
             UNUSED,             // flags
             (sockaddr*) &conn,  // dest_addr
             sizeof(conn));
      nbytes_sent += payload_size;

      // record info in case packet is not ACK'd
      unackedPacketsInfo.emplace_back(transmission_time, packet);
    }
    sendInfo.nextToSend += nbytes_to_send;
  }

  void TcpSocket::resendOldData() {
    std::chrono::time_point<CLOCK> now = CLOCK::now();
    size_t timeout = rttEstimate + (rttDevEstimate >> 2);  // >> 2 is times 4

    std::list<JC::UnackedPacketInfo>::iterator it = unackedPacketsInfo.begin();
    while (it != unackedPacketsInfo.end()) {
      void* packet = static_cast<void*>(it->getPacket());
 
      // It may have already been ACK'd
      JC::TcpHeader* hdr = static_cast<JC::TcpHeader*>(packet);
      if (hdr->seqNum < sendInfo.lastAck) {
        it = unackedPacketsInfo.erase(it);
        continue;
      }

      std::chrono::milliseconds elapsed_time
        = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->getTransmissionTime());
      if (elapsed_time.count() < timeout) {
        // if this packet's timer hasn't expired,
        // subsequent packets' haven't either
        break;
      }

      // some header fields of retransmitted packets change
      hdr->ackNum = recvInfo.nextExpected;
      hdr->advertisedWindow = recvInfo.getAdvertisedWindow();
      sendto(udpSocket,
             packet,
             hdr->packetLen,
             UNUSED,         // flags
             (sockaddr*) &conn,
             sizeof(conn));
      
      it->setRetransmitted();  // retransmitted=true
      it++;
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
      ERROR("Backend should not block indefinitely to received data.");
      return;
    }

    // wait some time in case no data has been received yet
    if (readMode == JC::ReadMode::TIMEOUT) {
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
      assert(recvdHdr.advertisedWindow > 0);

      // Update sending info
      sendInfo.otherSideAdvWindow = recvdHdr.advertisedWindow;
      if (recvdHdr.flags & JC_TCP_ACK_FLAG) {
        if (sendInfo.lastAck > recvdHdr.ackNum) {
          std::stringstream ss;
          ss << "receiveIncomingData:" << std::endl;
          ss << "\tlastAck=" << sendInfo.lastAck
                           << " > "
                           << recvdHdr.ackNum << "=recvdAckNum" << std::endl;
          ERROR(ss.str());
          assert(false);
          return;
        }

        sendInfo.lastAck = recvdHdr.ackNum;
      }

      bool arrived_in_order = recvdHdr.seqNum == recvInfo.nextExpected;
      size_t payload_len = recvdHdr.packetLen - recvdHdr.headerLen;
      uint32_t last_seq_num = recvdHdr.seqNum + payload_len - 1;
      if (last_seq_num - recvInfo.nextToRead < BUF_CAP) {
        {  // begin critical section
          // save payload into recvBuf
          std::lock_guard<std::mutex> received_lock_guard{receivedMutex};
          for (int i = 0; i < payload_len; i++) {
            size_t idx = (recvdHdr.seqNum + i) % BUF_CAP;
            recvBuf[idx] = recvdPacket[recvdHdr.headerLen + i];
            
            if (!arrived_in_order) {
              yetToAck[idx] = true;  // mark out-of-order bytes
            }
          }
          recvInfo.lastReceived = std::max(recvInfo.lastReceived,
                                            last_seq_num);

          /* NOTE: frontend uses nextExpected to know last bytes
                   available to read, it must be updated in critical section */
          if (arrived_in_order) {
            // nextExpected must be updated when packets arrive in order.
            recvInfo.nextExpected = recvdHdr.seqNum + payload_len;
            while (yetToAck[recvInfo.nextExpected % BUF_CAP]) {
              yetToAck[(recvInfo.nextExpected++) % BUF_CAP] = false;
            }
          }
        }  // end critical section

        if (arrived_in_order) {  // must send ACK
          receivedCondVar.notify_all();  // notify TCP's frontend: new data to read
          JC::TcpHeader ackHdr{JC_TCP_IDENTIFIER,
                               myPort,                          // srcPort
                               ntohs(conn.sin_port),            // destPort
                               UNUSED,                          // seqNum
                               recvInfo.nextExpected,          // ackNum
                               sizeof(JC::TcpHeader),           // headerLen
                               sizeof(JC::TcpHeader),           // packetLen
                               JC_TCP_ACK_FLAG,
                               recvInfo.getAdvertisedWindow(),  // advertisedWindow
                               UNUSED};                         // extensionLen

          /* NOTE: sendto blocks when it's send buffer is full, 
                   unless in non-blocking I/O mode.
                   So, that's why it's outside critical section */
          sendto(udpSocket,         // sockfd
                 static_cast<void*>(&ackHdr),  // buf
                 sizeof(JC::TcpHeader),        // len
                 0,                 // flags
                 (sockaddr*) &conn,  // dest_addr
                 sizeof(conn));     // addrlen
        }
      }
    }
  }
}

