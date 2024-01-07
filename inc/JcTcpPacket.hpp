#ifndef JC_TCP_PACKET_HPP
#define JC_TCP_PACKET_HPP

#include <cstdint>

#define IDENTIFIER 69420

struct JcTcpHeader {
  uint32_t identifier;
  uint16_t srcPort;
  uint16_t destPort;
  uint32_t seqNum;
  uint32_t ackNum;
  uint16_t headerLen;
  uint16_t packetLen;
  uint8_t flags;
  uint16_t advertisedWindow;
  uint16_t extensionLen;
  uint8_t extensionData[];
};

#endif  // JC_TCP_PACKET_HPP

