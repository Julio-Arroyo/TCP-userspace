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
        if (type == JC::SocketType::TCP_INITIATOR) {
          // Initiator requested connection to send data
          // ==> should notify Listener when there is no more data to send
          JC::TcpHeader finSegment{JC_TCP_IDENTIFIER,
                                   myPort, ntohs(conn.sin_port),   /* src_port, dest_port */
                                   UNUSED, recvInfo.nextExpected,  /* seqNum, ackNum */
                                   sizeof(JC::TcpHeader),          /* headerLen */ 
                                   sizeof(JC::TcpHeader),          /* packetLen */
                                   JC_TCP_FIN_FLAG,
                                   recvInfo.getAdvertisedWindow(),
                                   UNUSED};                        /* extensionLen */
          sendto(udpSocket, static_cast<void*>(&finSegment),  /* file des, buf */
                 sizeof(JC::TcpHeader), UNUSED,               /* len, flags */
                 (sockaddr*) &conn, sizeof(conn));             /* dest_addr, dest_len */
        }
        // LOG("Closing backend...");
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
        assert(hdr->seqNum + (hdr->packetLen - 1 - hdr->headerLen) < sendInfo.lastAck);
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
      
      it->setRetransmitted(now);  // retransmitted=true
      it++;
    }
  }

  void TcpSocket::receiveIncomingData(const JC::ReadMode read_mode) {
    if (read_mode != JC::ReadMode::TIMEOUT &&
        read_mode != JC::ReadMode::NO_WAIT) {
      ERROR("receiveIncomingData(): read_mode should be one of {TIMEOUT, NO_WAIT}");
      return;
    }

    if (read_mode == JC::ReadMode::TIMEOUT) {  // block for some time
      struct pollfd pfd = {.fd=udpSocket, .events=POLLIN /* there is data to read */};
      poll(&pfd, 1 /* num pollfd's */, ACK_TIMEOUT /* ms */);
    }

    // exit if there isn't at least a packet to read
    JC::TcpHeader recvd_hdr;
    socklen_t conn_len = sizeof(conn);
    int flags = MSG_DONTWAIT |  /* make non-blocking call */
                MSG_PEEK;       /* next recvfrom() call will return same data */
    if (((int) sizeof(JC::TcpHeader)) > recvfrom(udpSocket,
                                                 (void*) &recvd_hdr,     /* buf */
                                                 sizeof(JC::TcpHeader),  /* len */
                                                 flags,                                
                                                 (sockaddr*) &conn,      /* src_addr */
                                                 &conn_len)) {           /* addrlen */
      return;
    }

    // extract entire packet (header and payload) from socket
    size_t nbytes_recvd{0};
    std::vector<uint8_t> recvd_packet(recvd_hdr.packetLen);
    while (nbytes_recvd < recvd_hdr.packetLen) {
      uint8_t* read_addr = recvd_packet.data() + nbytes_recvd;
      nbytes_recvd += recvfrom(udpSocket,
                               static_cast<void*>(read_addr),
                               recvd_hdr.packetLen - nbytes_recvd,
                               UNUSED,  // flags
                               (sockaddr*) &conn,
                               &conn_len);
    }
    assert(recvd_hdr.identifier == JC_TCP_IDENTIFIER);
    assert(recvd_hdr.advertisedWindow > 0);

    if (recvd_hdr.flags & JC_TCP_FIN_FLAG && type == JC::TCP_LISTENER) {
      /* this side (initiator) will shutdown because
         other side (initiator) has no more data to send */
      std::lock_guard<std::mutex> close_lock_guard{closeMutex};
      dying = true;
      receivedCondVar.notify_all();  // notify frontend it's dying
    }

    // update sending info
    sendInfo.otherSideAdvWindow = recvd_hdr.advertisedWindow;
    if (recvd_hdr.flags & JC_TCP_ACK_FLAG) {
      assert(sendInfo.lastAck <= recvd_hdr.ackNum);
      sendInfo.lastAck = recvd_hdr.ackNum;

      // update rttEstimate
      std::list<JC::UnackedPacketInfo>::iterator it = unackedPacketsInfo.begin();
      while (it != unackedPacketsInfo.end()) {
        if (!it->getRetransmitted()) {
          JC::TcpHeader* curr_hdr = (JC::TcpHeader*) (it->getPacket());
          uint16_t curr_payload_len = curr_hdr->packetLen - curr_hdr->headerLen;

          if (curr_hdr->seqNum + curr_payload_len == recvd_hdr.ackNum) {
            std::chrono::time_point<CLOCK> now = CLOCK::now();
            std::chrono::milliseconds rtt_measured 
              = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->getTransmissionTime());
            std::stringstream ss;
            ss << "Measured: " << rtt_measured.count() << ". Updating RTT parameters: " << rttEstimate << "," << rttDevEstimate;
            updateEstimatesRTT(rtt_measured.count(), rttEstimate, rttDevEstimate);
            ss << " to (" << rttEstimate << "," << rttDevEstimate << ")";
            DEBUG(ss.str());
            break;
          }
        }
        it++;
      }
    }

    size_t payload_len = recvd_hdr.packetLen - recvd_hdr.headerLen;
    uint32_t last_seq_num = recvd_hdr.seqNum + payload_len - 1;
    /* Ignore payload-carrying packets that have already been received */
    if (recvd_hdr.seqNum != 0 && recvd_hdr.seqNum < recvInfo.nextExpected) {
      // assert starting and ending seqNum's in packet do NOT straddle lastAck
      assert(last_seq_num < recvInfo.nextExpected);  
      return;
    }
    if (recvd_hdr.seqNum > recvInfo.lastReceived &&
        (last_seq_num - recvInfo.nextToRead) >= BUF_CAP) {
      LOG("Out of capacity");
      return;
    }
    bool arrived_in_order = recvd_hdr.seqNum == recvInfo.nextExpected;

    {  // begin critical section: write to recvBuf
      std::lock_guard<std::mutex> recv_lock_guard{receivedMutex};
      for (int i = 0; i < payload_len; i++) {
        size_t idx = (recvd_hdr.seqNum + i) % BUF_CAP;
        recvBuf[idx] = recvd_packet[recvd_hdr.headerLen + i];

        if (!arrived_in_order) {
          assert(recvd_hdr.seqNum > recvInfo.nextExpected);
          /* this doesn't need to happen in critical section but
           * no point in looping again afterwards */
          yetToAck[idx] = true;
        }
      }

      /* update nextExpected (must be in critical section bc
       * it's used by frontend) */
      if (arrived_in_order) {
        recvInfo.nextExpected = last_seq_num + 1;
        uint32_t debug_tmp = recvInfo.nextExpected;

        // rolldown nextExpected over yetToAck
        while (yetToAck[recvInfo.nextExpected % BUF_CAP]) {
          yetToAck[recvInfo.nextExpected % BUF_CAP] = false;
          recvInfo.nextExpected++;
        }

        if (debug_tmp != recvInfo.nextExpected) {
          std::stringstream ss;
          ss << "Rolldown from " << debug_tmp << " to " << recvInfo.nextExpected;
          LOG(ss.str());
        }
      }
    }  // end critical section: write to recvBuf

    recvInfo.lastReceived = std::max(recvInfo.lastReceived, last_seq_num);
    if (arrived_in_order) {
      receivedCondVar.notify_all();  // frontend notified new data in recvBuf

      // send ACK
      transmitData(nullptr, 0,
                   UNUSED /* seq_num */, recvInfo.nextExpected,
                   JC_TCP_ACK_FLAG, UNUSED /* sendto_flags */);
    }
  }
}

