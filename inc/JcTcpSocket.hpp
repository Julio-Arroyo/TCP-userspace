#ifndef JC_TCP_SOCKET_HPP
#define JC_TCP_SOCKET_HPP

#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // htons, inet_addr
#include <cstdint>
#include <cstring>  // std::memset
#include <string>
#include <mutex>
#include "JcTcpSocketBackend.hpp"

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

    /**
     * @brief Constructs a JC-TCP socket
     *
     * The functionality depends on 'socket_type'.
     *   - INITIATOR: 'server_ip' and 'port' constitute the
     *                address being connected to. This socket
     *                will be bound to a random usable free port
     *                with INADDR_ANY.
     *   - LISTENER: 'server_ip' and 'port' constitute the
     *               address this socket is bound to.
     */
    void open(const JC::SocketType socket_type,
              const int port,
              const std::string& server_ip);

    int read();
    int write();
    int close();

  private:
    std::thread backendThread;

    int udpSocket;
    uint16_t my_port;
    JC::SocketType type;

    /* If 'type' is TCP_INITIATOR, 'conn' is the (ip, port) this socket
     * is connected to.
     * If it is TCP_LISTENER, 'conn' is the (ip, port) this socket is
     * bound to */
    sockaddr_in conn;  

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

