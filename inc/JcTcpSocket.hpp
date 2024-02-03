#ifndef JC_TCP_SOCKET_HPP
#define JC_TCP_SOCKET_HPP

#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // htons, inet_addr
#include <stdexcept>     // invalid_argument
#include <algorithm>     // std::copy, std::min
#include <unistd.h>      // close
#include <iostream>
#include <sstream>       // std::stringstream
#include <utility>       // std::move
#include <cstdint>
#include <cstdlib>       // std::rand
#include <cstring>       // std::memset, strerror
#include <cassert>       // assert()
#include <poll.h>        // poll
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <list>

#include "JcTcpPacket.hpp"
#include "Config.hpp"
#include "Util.hpp"

#define JC_EXIT_SUCCESS 0
#define JC_EXIT_FAILURE -1

#define UNUSED 0

#define MAX_PAYLOAD_SIZE (MAX_PACKET_LEN - sizeof(JC::TcpHeader))

#define LOG(msg) std::cout << "[LOG]: " << msg << std::endl
#define ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl

// ACK ~ receiver sets ackNum to the nextExpected seqNum

namespace JC {

  /**
   * Set of seqNum's maintained by the sending side of the connection.
   */ 
  struct SendInfo {
    /**
     * - Corresponds to latest (and largest) ackNum received by the
     *   sending side of the connection.
     * - Serves as backend's read index into sendBuf.
     */
    uint32_t lastAck;

    uint32_t nextToSend;

    /* Serves as frontend's write index into sendBuf. */
    uint32_t nextToWrite;
 
    /* Receiver side's advertised window size. */
    uint16_t otherSideAdvWindow;
  };

  /**
   * Set of seqNum's maintained by the receiver side of the connection.
   */ 
  struct RecvInfo {
    /* Frontend's read index into recvBuf. */
    uint32_t nextToRead;

    /**
     * Smallest sequence number 'x' such that, for all i < x, sequence 
     * number 'i' has already been received.
     */
    uint32_t nextExpected;

    /* If packet w/ seqNum = 650 is received with payload_len = 100,
     * then lastReceived = 749 */
    uint32_t lastReceived;

    uint16_t getAdvertisedWindow() {
      return FIXED_WINDOW;

      // SWP
      uint16_t num_buffered_bytes = lastReceived + 1 - nextToRead;
      assert(num_buffered_bytes <= BUF_CAP);
      return BUF_CAP - num_buffered_bytes;
    }
  };

  enum SocketType {
    TCP_INITIATOR = 0,  // client
    TCP_LISTENER = 1    // server
  };

  /* Waiting policy when reading from a JC-TCP socket. */
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
     *
     * @param socket_type Listener or initiator
     * @param port to either connect to or bind to (depending on socket_type)
     * @param server_ip Address to connect to (only used if socket is initiator)
     *
     * @return 0 on success, -1 on failure
     */
    int open(const JC::SocketType socket_type,
              const int port,
              const std::string& server_ip);

    /**
     * @brief Reads data from the JC-TCP socket.
     *
     * Any data available in the socket buffer is placed into 'dest_buf'.
     *
     * @param dest_buf Where the data received on the socket is written to
     * @param len Max number of bytes to read.
     * @param read_mode Indicates how to wait on data to read
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
    int teardown();

  private:
    /*** BACKEND THREAD FUNCTIONALITY ***/
    /**
     * @brief Launches thread running JC-TCP backend main routine.
     */
    void beginBackend();

    /**
     * @brief Transmits bytes that have not yet been transmitted.
     *
     * The data to be sent is split into individual packets, and those
     * are transmitted while honoring the receiver side's advertised
     * window size.
     *
     * @param num_unsent_bytes Size of data that has not been transmitted
     *                         at all.
     */
    void sendNewData(size_t num_unsent_bytes);

    /**
     * @brief Retransmits packets that aren't ACKed within a time window.
     *
     * The timeout period is computed as:
     *   $ 1*[rttEstimate] + 4*[rttDevEstimate]
     */
    void resendOldData();

    /**
     * Checks udpSocket for any incoming data. If it sees anything,
     * it puts it into receivedBuf.
     *
     * Called in backend tight loop.
     *
     * @param readMode Dictates whether this call should set a timer to
     *                 wait for data, or try to read immediately.
     */
    void receiveIncomingData(const JC::ReadMode readMode);

    std::thread backendThread;
    /*** END OF BACKEND THREAD FUNCTIONALITY ***/

    /* Backend constantly checks udpSocket for incoming data, and puts
     * anything it received into recvBuf. read() then directly
     * retrieves data from receivedBuf */
    std::array<uint8_t, BUF_CAP> recvBuf;
    JC::RecvInfo recvInfo{0, 0, 0};
    std::mutex receivedMutex;
    // backend signals frontend when it puts data in recvBuf
    std::condition_variable receivedCondVar;  
    // Corresponds to data in recvBuf that arrived out-of-order and thus
    // has not been ACKed.
    std::array<bool, BUF_CAP> yetToAck;

    /* write() puts data into sendBuf, backend empties it and
     * sends it via the udpSocket */
    std::array<uint8_t, BUF_CAP> sendBuf;
    JC::SendInfo sendInfo{0, 0, 0};
    std::mutex writeMutex;  // synch app & JC-TCP backend when modifying sendBuf

    int udpSocket;
    uint16_t myPort{0};
    JC::SocketType type;

    /* If 'type' is TCP_INITIATOR, 'conn' is the (ip, port) this socket
     * is connected to.
     * If it is TCP_LISTENER, 'conn' is the (ip, port) this socket is
     * bound to */
    sockaddr_in conn;  

    // whether connection is ready to be closed. Only close() modifies this flag
    bool dying{false};  
    std::mutex closeMutex;

    /** Parameters for RTT estimation. */
    size_t rttEstimate{ACK_TIMEOUT};
    size_t rttDevEstimate{ACK_TIMEOUT / 16};  // Round-Trip Time deviation

    /**
     * Metadata used for triggering lost-packet retransmission and RTT estimation.
     */
    std::list<JC::UnackedPacketInfo> unackedPacketsInfo;
  };
}

#endif  // JC_TCP_SOCKET_HPP

