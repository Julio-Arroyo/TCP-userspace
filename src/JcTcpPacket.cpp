#include "JcTcpPacket.hpp"

void JC::initHeader(JC::TcpHeader* hdr,
                    const uint16_t src_port,
                    const uint16_t dest_port,
                    const uint32_t seq_num,
                    const uint32_t ack_num, 
                    const uint16_t header_len,
                    const uint16_t packet_len,
                    const uint8_t flags_,
                    const uint16_t advertised_window,
                    const uint16_t extension_len) {
  hdr->identifier = JC_TCP_IDENTIFIER;
  hdr->srcPort = src_port;
  hdr->destPort = dest_port;
  hdr->seqNum = seq_num;
  hdr->ackNum = ack_num;
  hdr->headerLen = header_len;
  hdr->packetLen = packet_len;
  hdr->flags = flags_;
  hdr->advertisedWindow = advertised_window;
  hdr->extensionLen = extension_len;
}
