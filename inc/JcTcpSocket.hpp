#ifndef JC_TCP_SOCKET_HPP
#define JC_TCP_SOCKET_HPP

#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // htons, inet_addr
#include <stdexcept>     // invalid_argument
#include <algorithm>     // std::copy, std::min
#include <utility>       // std::move
#include <cstdint>
#include <cstring>       // std::memset
#include <cassert>
#include <poll.h>        // poll
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#include "JcTcpPacket.hpp"
#include "Config.hpp"

#define JC_EXIT_SUCCESS 0
#define JC_EXIT_FAILURE -1

#define UNUSED 0

const size_t MaxPayloadSize = MAX_PACKET_SIZE - sizeof(JC::TcpHeader);

namespace JC {
  struct SendState {
    uint32_t lastAck;
    uint32_t lastSent;
    uint32_t lastWritten;
  };

  struct RecvState {
    uint32_t lastRead;
    uint32_t nextExpected;
    uint32_t lastReceived;
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

    /**
     * @brief Reads data from the socket.
     *
     * @param dest_buf Where the data received on the socket is written to
     * @param len Max number of bytes to read.
     *
     * @return The number of bytes read on success, -1 on failure
     */
    int read(void* dest_buf, const int len, const JC::ReadMode read_mode);

    /**
     * @brief Writes data to the socket.
     *
     * @param src_buf Buffer with data to be written
     * @param write_len Number of bytes to write
     *
     * @return 0 on success, -1 on failure
     */
    int write(void* src_buf, const int write_len);

    /**
     * @brief Close a JC-TCP socket
     *
     * @return 0 on success, -1 on failure
     */
    int close();

  private:
    /*** BACKEND METHODS RUNNING IN 'backendThread' ***/

    /**
     * @brief JC-TCP backend main routine
     */
    void beginBackend();

    /**
     * Splits a stream of data into individual packets
     * and sends them one at a time.
     *
     * @param dataToSend Raw bytes to be transmitted over the network.
     */
    void sendOnePacketAtATime(std::vector<uint8_t>& dataToSend);

    /**
     * Checks udpSocket for any incoming data. If it sees anything,
     * it puts it into received_buf.
     *
     * Called after sending when waiting for ACK  // TODO fix stop-wait
     * Called in backend tight loop
     *
     * @param readMode Dictates whether this call should set a timer to
     *                 wait for data, or try to read immediately.
     */
    void receiveIncomingData(const JC::ReadMode readMode);

    std::thread backendThread;

    int udpSocket;
    uint16_t my_port;
    JC::SocketType type;

    /* If 'type' is TCP_INITIATOR, 'conn' is the (ip, port) this socket
     * is connected to.
     * If it is TCP_LISTENER, 'conn' is the (ip, port) this socket is
     * bound to */
    sockaddr_in conn;  

    /* Backend constantly checks udpSocket for incoming data, and puts
     * anything it received into received_buf. read() then directly
     * retrieves data from received_buf */
    std::vector<uint8_t> received_buf;
    // int received_len;
    std::mutex read_mutex;
    std::condition_variable wait_cond;

    /* write() puts data into sending_buf, backend empties it and
     * sends it via the udpSocket */
    std::vector<uint8_t> sending_buf;
    // int sending_len;
    std::mutex write_mutex;  // synch app & JC-TCP backend

    // whether connection is ready to be closed. Only close() modifies this flag
    bool dying{false};  
    std::mutex close_mutex;

    JC::SendState sendState;
    JC::RecvState recvState;
  };
}

#endif  // JC_TCP_SOCKET_HPP

