#ifndef JC_TCP_SOCKET_HPP
#define JC_TCP_SOCKET_HPP

#include <cstdint>
#include <mutex>
#include <condition_variable>

namespace JC {
  struct Window {
    uint32_t next_seq_expected;
    uint32_t last_ack_received;
  };

  enum SocketType {
    TCP_INITIATOR = 0,
    TCP_LISTENER = 1
  };

  enum ReadMode {
    BLOCK = 0,
    NO_WAIT,
    TIMEOUT,
  };

  class TcpSocket {
  public:
    TcpSocket();

    void start();
    int read();
    int write();
    int close();

  private:
    int socket;
    uint16_t my_port;
    sockaddr_in conn;
    JC::SocketType type;

    uint8_t* received_buf;
    int received_len;
    std::mutex read_mutex;
    std::condition_variable wait_cond;

    uint8_t* sending_buf;
    int sending_len;
    std::mutex write_mutex;  // synch app and JC-TCP backend

    int dying;
    std::mutex close_mutex;
    JC::Window window;
  };
}

#endif  // JC_TCP_SOCKET_HPP

