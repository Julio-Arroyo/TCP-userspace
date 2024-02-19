#ifndef JC_TCP_UTIL_HPP
#define JC_TCP_UTIL_HPP

#include <cassert>
#include <chrono>
#include <vector>

#define CLOCK std::chrono::high_resolution_clock

namespace JC {
  /**
   * Used by TcpSocket for
   *   1. Retransmission of unacked packets
   *   2. RTT estimation solely using packets that have not been
   *      retransmitted.
   */
  class UnackedPacketInfo {
  public:
    UnackedPacketInfo(std::chrono::time_point<CLOCK> transmission_time, std::vector<uint8_t>& pkt);

    uint8_t* getPacket();

    std::chrono::time_point<CLOCK> getTransmissionTime();

    void setRetransmitted(std::chrono::time_point<CLOCK> retransmission_time);

    bool getRetransmitted();

  private:
    std::chrono::time_point<CLOCK> transmissionTime;
    std::vector<uint8_t> packet;
    bool retransmitted{false};
  };


  void updateEstimatesRTT(size_t rtt_measured,
                          size_t& rtt_estimate,
                          size_t& rtt_dev_estimate);
}

#endif

