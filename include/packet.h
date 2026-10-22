#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_HEADER_LEN  512
#define MAX_BODY_LEN    1024
#define MAX_PATH_LEN    128
#define MAC_ADDR_LEN    18
#define IP_ADDR_LEN     16

// Application Layer Protocols
typedef enum {
    PROTO_HTTP = 0,
    PROTO_DNS,
    PROTO_CDN,
    PROTO_SMTP,
    PROTO_P2P
} AppProtocol;

// Sub-protocol for Mail (SMTP transfer vs Retrieval)
typedef enum {
    MAIL_SUB_RAW_SMTP = 0,
    MAIL_SUB_POP3,
    MAIL_SUB_IMAP,
    MAIL_SUB_WEBMAIL
} MailSubProtocol;

// Direction of packet travel
typedef enum {
    DIR_FORWARD = 0, // Client -> Server (Request)
    DIR_REVERSE      // Server -> Client (Response)
} PacketDirection;

// L7: Application Layer
typedef struct {
    AppProtocol protocol;
    MailSubProtocol mail_subprotocol;
    char method_or_cmd[32];   // e.g. "GET", "POST", "QUERY", "MAIL FROM", "GET_CHUNK"
    char target[MAX_PATH_LEN]; // e.g. "/index.html", "example.com", "video.mp4", "alice@test.com"
    char headers[MAX_HEADER_LEN];
    char body[MAX_BODY_LEN];
    
    // Server Response fields
    int status_code;          // e.g. 200, 404, 250
    char status_message[64];  // e.g. "OK", "Not Found", "Queued"
    char response_body[MAX_BODY_LEN];
} L7_Payload;

// L4: Transport Layer
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t flags;            // SYN=0x02, ACK=0x10, FIN=0x01, PSH=0x08
    uint8_t transport_proto;  // 6 = TCP, 17 = UDP
    L7_Payload payload;
} L4_Segment;

// L3: Network Layer
typedef struct {
    char src_ip[IP_ADDR_LEN];
    char dest_ip[IP_ADDR_LEN];
    uint8_t ttl;
    uint8_t protocol;         // 6 = TCP, 17 = UDP
    L4_Segment segment;
} L3_Packet;

// L2: Data Link Layer
typedef struct {
    char src_mac[MAC_ADDR_LEN];
    char dest_mac[MAC_ADDR_LEN];
    uint16_t ethertype;       // 0x0800 = IPv4
    L3_Packet packet;
} L2_Frame;

// Top-level Packet wrapper carried across queues
typedef struct {
    PacketDirection direction;
    int simulation_id;
    L2_Frame frame;
} Packet;

#endif // PACKET_H
