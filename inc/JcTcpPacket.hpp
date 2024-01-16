#ifndef JC_TCP_PACKET_HPP
#define JC_TCP_PACKET_HPP

#include <cstdint>

#define JC_TCP_IDENTIFIER 69420

// JC::TcpHeader flags
#define JC_TCP_SYN_FLAG 0x8
#define JC_TCP_ACK_FLAG 0x4
#define JC_TCP_FIN_FLAG 0x2

namespace JC {
  struct TcpHeader {
    uint32_t identifier;
    uint16_t srcPort;
    uint16_t destPort;
    uint32_t seqNum;

    // valid when ACK flag is set. Identifies next seqNum sender expects.
    uint32_t ackNum;

    uint16_t headerLen;
    uint16_t packetLen;  // Length (header + payload) in bytes
    uint8_t flags;
    uint16_t advertisedWindow;
    uint16_t extensionLen;
    // uint8_t extensionData[];  // TODO
  };

  void initHeader(JC::TcpHeader* hdr,
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
}

#endif  // JC_TCP_PACKET_HPP

