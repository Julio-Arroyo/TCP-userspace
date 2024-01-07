#ifndef JC_TCP_HPP
#define JC_TCP_HPP

#include "JcTcpSocket.hpp"

/**
 * @brief Constructs a JC-TCP socket
 *
 * An initiator socket is used to connect to a listener socket.
 *
 * @param sock initialized by this function.
 * @param socket_type Listener or initiator
 * @param port to either connect to or bind to (depending on socket_type)
 * @param server_ip Address to connect to (only used if socket is initiator)
 *
 * @return 0 on success, -1 on failure.
 */
int jc_socket(JC::TcpSocket& sock,
              const JC::SocketType socket_type,
              const int port,
              const std::string& server_ip = "");

int jc_close(JC::TcpSocket& sock);

/**
 * @brief Reads from a JC-TCP socket
 *
 * Any data available in the socket buffer is placed into 'dest_buf'.
 *
 * @param sock To be read from
 * @param dest_buf Destination buffer
 * @param len Max number of bytes to be read.
 * @param read_mode Indicates how to wait on data to read
 *
 * @return Number of bytes read on success, -1 on failure
 */
int jc_read(JC::TcpSocket& sock,
            void* dest_buf,
            const int len,
            const JC::ReadMode read_mode);

/**
 * @brief Reads from a JC-TCP socket
 *
 * @param sock To be written to
 * @param src_buf Data to write
 * @param len Number of bytes to write
 *
 * @return 0 on success, -1 on failure
 */
int jc_write(JC::TcpSocket& sock, void* buf, const int len);

#endif

