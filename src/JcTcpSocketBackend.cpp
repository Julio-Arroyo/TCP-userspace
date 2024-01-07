#include "JcTcpSocketBackend.hpp"

void handle_incoming_data(JC::TcpSocket& sock, uint8_t* packet) {
  // two cases
  //   case ACK msg:
  //   default:  // receiving data
}

void check_for_incoming_data(JC::TcpSocket& sock, const JC::ReadMode read_mode) {
  switch (read_mode) {
    case JC::ReadMode::TIMEOUT: {  // wait for ACK
      // poll
    }
    case JC::ReadMode::NO_WAIT: {
      // read hdr
    }
    default:
      break;
  }
  // read rest of packet
  // handle_message()
}

void send_one_pkt_at_a_time(JC::TcpSocket& sock, uint8_t* data, int len) {

  // split into individual packets
  //
  // loop forever
    //   send one packet
    //   check_for_data()  // wait for ACK
    //   break if acked
}

void* begin_backend(void* in) {
  // infinite loop
  //    close if dying and no more data to be sent
  //
  //    send_one_pkt_at_a_time();
  //
  //    receive data
}

