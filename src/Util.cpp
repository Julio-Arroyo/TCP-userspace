#include "Util.hpp"

JC::UnackedPacketInfo::UnackedPacketInfo(std::chrono::time_point<CLOCK> transmission_time,
                                         std::vector<uint8_t>& pkt) {
  transmissionTime = transmission_time;
  packet = std::move(pkt);
  retransmitted = false;
  assert(pkt.empty());
}

uint8_t* JC::UnackedPacketInfo::getPacket() {
  return packet.data();
}

std::chrono::time_point<CLOCK> JC::UnackedPacketInfo::getTransmissionTime() {
  return transmissionTime;
}

void JC::UnackedPacketInfo::setRetransmitted(std::chrono::time_point<CLOCK> retransmission_time) {
  retransmitted = true;
  transmissionTime = retransmission_time;
}

bool JC::UnackedPacketInfo::getRetransmitted() {
  return retransmitted;
}

void JC::updateEstimatesRTT(size_t rtt_measured,
                            size_t& rtt_estimate,
                            size_t& rtt_dev_estimate) {
  int rtt_error = rtt_measured - rtt_estimate;
  int rtt_correction = rtt_error >> 3;  // division by 8

  int abs_rtt_error = rtt_error;
  if (rtt_error < 0) {
    abs_rtt_error = -rtt_error;  // make abs_rtt_error positive

    // rtt_estimate is unsigned, so correction shouldn't make it negative
    // Easy to prove the condition below is true, but assert to be sure :)
    assert(static_cast<int>(rtt_estimate) >= -1*(rtt_correction));
  }
  int rtt_dev_delta = abs_rtt_error - rtt_dev_estimate;
  int rtt_dev_correction = rtt_dev_delta >> 3;
  if (rtt_dev_correction < 0) {
    // Easy to prove the condition below is true, but assert to be sure :)
    assert(static_cast<int>(rtt_dev_estimate) >= -1*rtt_dev_correction);
  }

  rtt_estimate += rtt_correction;
  rtt_dev_estimate += rtt_dev_correction;
}

