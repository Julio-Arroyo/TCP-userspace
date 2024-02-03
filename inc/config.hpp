#ifndef JC_TCP_CONFIG_HPP
#define JC_TCP_CONFIG_HPP

#define MAX_PACKET_LEN 1400
#define FIXED_WINDOW (2 * MAX_PACKET_LEN)
#define BUF_CAP (MAX_PACKET_LEN * 10)  // capacity of sendBuf and recvBuf

#define ACK_TIMEOUT 3000

#define LOOPBACK_ADDR "127.0.0.1"

// Server-Client configs
#define SERVER_IP_ADDR LOOPBACK_ADDR
#define SERVER_PORT 2345  // arbitrary

#endif  // JC_TCP_CONFIG_HPP

