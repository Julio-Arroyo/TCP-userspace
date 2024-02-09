# TCP-userspace

## What is it?
TCP is a networking protocol. It is connection-oriented and guarantees
reliable & in-order delivery of data.

## Implementation design
Use UDP sockets as the byte-transmission mechanism, and implement myself
the delivery-guarantee and in-order-delivery features.

API: open, read, write, teardown.

### How does it work under the hood?
There are two threads, **frontend** and **backend**.
- The **frontend** refers to the JC-TCP API, which of course runs in the
  thread of the process using the JC-TCP sockets.
- The **backend** contains the core logic, and is responsible for
  transmitting data and guaranteeing its in-order delivery.

The **frontend** and the **backend** communicate via two buffers, one
for sending and one for receiving data.

Thus, when a program invokes write(buf, nbytes), all that takes place in that
operation in that thread (in the **frontend**) is that 'nbytes' from 'buf' are copied into 'sendBuf'.
Similarly, when a program invokes read(buf, nbytes, read_mode), all that takes
place is that up to 'nbytes' are copied from 'recvBuf' into 'buf'. That means
that NO actual data is being transmitted over the network at the time of
calling read/write.

The actual data transmission and congestion control takes place in the
**backend**. In an infinite loop, it checks whether the **frontend** placed
any data in 'sendBuf'. If so it puts it in individual packets and
transmits those to the other side of the connection using the underlying UDP
sockets. There is also logic to retransmit those same packets in case the
other side of the connection does not ACK them. Similarly, it checks the
underlying UDP socket to see if the other side of the connection has sent
any data. If so, it buffers it in 'recvBuf' and signals the **frontend**
there is data available to read as long as the data arrived in-order.

## Philosophy
- Easy to read code, comments explaining literally what it does.
- Conceptual explanations of what each component does.
- Concept and implementation as close as possible.

## Sliding Window Protocol
Receiver Side (RS) maintains a 'yetToAck' binary array of same size as
'receiveBuffer'

'''
onPacket(pkt):
    remaining_capacity = CAPACITY - 
'''

