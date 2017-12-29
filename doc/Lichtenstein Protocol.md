# Lichtenstein Protocol
The lichtenstein protocol allows for a single central server to provision, control and otherwise interact with lichtenstein LED drivers over an IP network such as Ethernet.

This protocol operates on port 7420 over the UDP transport, and makes use of multicast (on a configurable multicast address, `239.42.0.69` by default) for discovery of devices on the local network. Unicast is used for all other communications between the client devices and server. Messages are represented as binary packed (i.e. no padding between fields) structs, with multi-byte sequences sent in network byte order.

## Packet Header
Each packet starts with the following header:

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint32_t | Magic Value (always `0x4c494348`)
| 4      | uint32_t | Protocol Version (currently `0x00010000`)
| 8      | uint32_t | Packet checksum [^checksum]
| 12     | uint16_t | Opcode
| 14     | uint16_t | Flags
| 16     | uint16_t | Sequence Number [^sequence]
| 18     | uint16_t | Total number of packets in sequence
| 20     | uint32_t | Transaction Number [^txn]
| 24     | uint32_t | Payload length (bytes) [^length]

A payload length of zero for opcodes that generate a response (such as the Node Status Request) indicate a request for that information, and will be responded to by the other end of the channel with the appropriate data.

## Flags
Flags are interpreted as a 16-bit wide bitfield, with bit 0 being the least significant bit, and bit 15 being the most significant. The following types of flags are defined:

| Bit | Note
| --: | ----
| 15  | Multicast packet (sent to all nodes on the network)
| 14  | Response (set if this packet was generated in response to a particular request; the opcode is not modified.)
| 13  | Acknowledgement; if the node is responding to a previous request, which completed successfully, but has no data to respond with. The payload length should be zero and no payload may be sent.
| 12  | Negative acknowledgement; a previous request could not be completed. The payload consists of an optional zero-terminated string that gives a reason.
| 0   | Payload checksum present

## Opcodes
The opcode in the packet determines its "type" and how the device should interpret the payload that follows, which may be zero length. The following opcodes are defined:

| Opcode | Note
| -----: | ----
| 0      | Node Announcement
| 1      | Server Announcement
| 2      | Node Adoption
| 3      | Node Status Request
| 4      | Framebuffer Data
| 5      | Node Configuration
| 6      | Sync Output
| 7      | Read GPIOs
| 8      | Write GPIOs
| 9      | System Reset Request
| 10     | System Sleep Request
| 11     | Keepalive
| 12     | Node Reconfiguration

All opcodes will only be processed if received on the node's actual IP address (i.e. sent as unicast) unless otherwise specified.

### Node Announcement
Node announcements are multicast by nodes when they have not been adopted, i.e. after they have been powered up or reset. They contain the following information:

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint32_t | Software Version
| 4      | uint32_t | Hardware Version
| 8      | 6 bytes  | Node MAC address
| 14     | uint16_t | Node port (the port on which the node expects to receive communications; this should always be 7420)
| 16     | uint32_t | Node IP address
| 20     | uint32_t | Total framebuffer memory (in bytes)
| 24     | uint16_t | Available light output channels
| 26     | uint16_t | Available GPIO digital inputs
| 28     | uint16_t | Available GPIO digital outputs
| 30     | uint16_t | Available GPIO analog inputs
| 32     | uint16_t | Available GPIO analog outputs
| 34     | uint16_t | Hostname Length
| 36     | char     | Zero-terminated hostname string

### Server Announcement
Server announcements are periodically multicast by the server on the network, no more frequently than every 15 seconds.

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint32_t | Software Version
| 4      | uint32_t | Server capabilities (reserved; is all zeros)
| 8      | uint32_t | Server IP Address
| 12     | uint16_t | Server port (the port on which the server expects to receive communications; this should always be 7420)
| 14     | uint16_t | Server status; should be zero if the server is operating normally, nonzero if an error condition exists. Nodes may ignore servers with a nonzero status.
| 16     | uint16_t | Hostname Length
| 18     | char     | Zero-terminated hostname string

Nodes are not required to act on the server announcements, or even so much as acknowledge them. They are provided simply as a means for nodes to discover what servers are available on the local network and display them to the user, if desired.

### Node Adoption
When a node is adopted by a server, the server will send a node adoption request. This establishes a "channel" between the node and server, i.e. the node will send all traffic to this server from now on, and exclusively accepts commands from this server. (This association can be broken by issuing a "system reset request" to the node or multicast address, or if the node does not receive any messages from the server in one minute.)

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint32_t | Server IP Address
| 4      | uint16_t | Server port (the port on which the server expects to receive communications; this should always be 7420)
| 6      | uint16_t | Flags (reserved; should be zero)
| 8      | uint32_t | Number of channels to provision
| 12     | uint32_t | Number of LEDs on first channel (repeated for every channel to be provisioned)

If the node has not been adopted, it will respond with a positive acknowledgement and enter the operating mode. If it has already been adopted, it will instead respond with a negative acknowledgement, along with a description of the server that has adopted it previously.

### Node Status Request
The server may periodically poll the node as to its current status via a node status request.

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint32_t | Uptime (in seconds)
| 4      | uint32_t | Total memory (in bytes)
| 8      | uint32_t | Used memory (in bytes)
| 12     | uint32_t | Received packets
| 16     | uint32_t | Sent packets
| 20     | uint32_t | Packet level checksum errors (packets that passed the UDP checksum check, as well as that of the underlying medium, but failed the CRC check included in the packet itself)
| 24     | uint32_t | Frames output
| 28     | uint16_t | Output state (see table below)
| 30     | uint16_t | CPU usage (on a scale of 1 to 100, how much the CPU has been utilized over the past 15 seconds)
| 32     | int32_t  | Average conversion time, in microseconds, of framebuffer data to output format (averaged over the past 15 seconds); -1 if not applicable
| 36     | uint32_t | Bytes the node received since the last reset [^byteCntrs]
| 40     | uint32_t | Bytes the node sent since the last reset [^byteCntrs]
| 44     | uint32_t | Number of symbols with an error received on the wire.
| 48     | uint32_t | Speed of the underlying network medium, in bits per second
| 52     | uint32_t | Duplex state of the underlying network medium; 0 if half-duplex, 1 for full duplex.

Output state may be one of the following:

| Value | Note
| ----: | ----
| 0     | Idle; no output is being produced
| 1     | Data is being output to at least one channel
| 2     | Conversion; data is being converted for output

### Framebuffer Data
Each node may have one or more framebuffers to store data to be output at a later time via a "sync output" packet. Data may have 3 (RGB) or 4 (RGBW) channels.

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint32_t | Framebuffer into which data is written (each framebuffer corresponds to exactly one output channel)
| 4      | uint32_t | Data format; see table below
| 8      | uint32_t | Number of elements following
| 12     | char     | Start of framebuffer data

The following data formats are supported:

| Format | Size per Element | Type
| -----: | ---------------- | ----
|      1 | 3 bytes          | RGB data
|      2 | 4 bytes          | RGBW data

When this packet is received, the data should be immediately copied into the framebuffer, blocking if the framebuffer is currently in use (by either an output or a conversion operation, depending on the hardware of the node.) If no conversion is needed, an acknowledgement may be sent immediately after data has been copied; otherwise, the acknowledgement must be delayed until the data is ready to be output.

### Sync Output
To synchronize all nodes' output, the server will multicast a sync output message to all nodes on the network.

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint32_t | Channel to output

Nodes will acknowledge this request immediately after they have begun the output of the stored data.

### Read GPIOs
_Todo_

### Write GPIOs
_Todo_

### System Reset Request
If a node needs to be reset (to reboot it to load new firmware, remove adoption status, or for another reason) a system reset request may be sent to it. There is no payload, and it may only be received by a specific node via unicast. The node will acknowledge the request and reset immediately after the acknowledgement packet has been transmitted.

### System Sleep Request
Requests that a node enters a low-power state, where it does not drive its outputs and shuts down peripherals to save power. The node will acknowledge the request, and immediately after the acknowledgement packet has been tramsitted, enter a low-power mode.

The node can be woken by sending any regular packet to it, i.e. its PHY must remain active and listening to activity on the network.

_Note_: The node should stop its de-adoption timer before entering the low-power mode so that it does not become orphaned when it wakes.

### Keepalive
If no data needs to be transmitted to the node, a keepalive packet may instead be sent. This allows the node to reset its internal adoption state timer, and provides evidence that the server is still alive: i.e. it indicates to the node that there is nothing to do, but it should remain in the operating mode. The node will simply acknowledge this packet.

### Node Reconfiguration
Nodes may be reconfigured on-the-fly by a server on the network to change some properties that affect their operation. These can all be changed at once with this packet type.

This data is written to nonvolatile memory in the node. Acknowledgement is provided once the data has been successfully commited to storage, but not necessarily once the settings have been applied.

| Offset | Size     | Value
| -----: | -------- | -----
| 0      | uint16_t | Hostname Length
| 2      | char     | Zero-terminated hostname string






[^sequence]: If a packet contains more data than can fit in a single packet (see disclaimer about length) the sequence number field can be used, and the "total packets in sequence" field should be set to a number greater than one.

[^checksum]: The contents of the packet may be protected by using an optional CRC32 with a polynomial of `0x04C11DB7` over all fields following the checksum field, if desired. It is recommended to enable checksums on all packets to guard against corruption during transit by [faulty network hardware.] (http://www.evanjones.ca/tcp-and-ethernet-checksums-fail.html) If the checksum is present, the appropriate flag _must_ be set. Nodes _must_ silently discard packets with invalid checksums and the checksum flag set. Nodes _must not_ discard packets with an invalid checksum if the checksum flag is not set.

[^txn]: Each packet must contain a random 32-bit transaction number. When responding to a request, the transaction number must be the same as that of the request that caused the response to be generated. This allows for client/server code to be simpler.

[^length]: Even though this field technically allows for a packet payload length of \\( 2^{32}-1 \\) bytes, it's not recommended to create packets that exceed the MTU of the underlying medium, usually 1500. Thus, conservatively, you should not send packets with a payload of more than 1400 bytes. There is no guarantee that clients can process packets larger than this, even if they are fragmented. Instead, use the sequence number facility to split the data.

[^byteCntrs]: Byte counters for received/transmitted bytes are divided by 64 before they are sent. This allows the counters to represent roughly 250GB of traffic, after which they will roll over. The node itself may store this data with a higher granularity, but the burden falls on the server to determine if the counter rolled over and update its accounting, if needed.
