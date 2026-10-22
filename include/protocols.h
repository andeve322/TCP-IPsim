#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include "packet.h"

// Initialize protocol defaults
void protocol_build_request(L7_Payload *payload, AppProtocol proto, MailSubProtocol subproto, const char *method, const char *target, const char *headers, const char *body);

// Process L7 request on Server node and generate coherent response
void protocol_process_server_request(L7_Payload *payload);

#endif // PROTOCOLS_H
