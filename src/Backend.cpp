#include "Backend.hpp"

void handle_message(JC::TcpSocket& sock, uint8_t* packet) {

}

void check_for_data(JC::TcpSocket& sock, const JC::ReadMode read_mode) {

}

void send_one_pkt_at_a_time(JC::TcpSocket& sock, uint8_t* data, int len) {

  // split into individual packets
  //
  // loop forever
    //   send one packet
    //   receive data with timeout
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

