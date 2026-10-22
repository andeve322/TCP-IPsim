#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "protocols.h"

void protocol_build_request(L7_Payload *payload, AppProtocol proto, MailSubProtocol subproto, const char *method, const char *target, const char *headers, const char *body) {
    if (!payload) return;
    memset(payload, 0, sizeof(L7_Payload));
    
    payload->protocol = proto;
    payload->mail_subprotocol = subproto;
    
    strncpy(payload->method_or_cmd, method ? method : "", sizeof(payload->method_or_cmd) - 1);
    strncpy(payload->target, target ? target : "", sizeof(payload->target) - 1);
    strncpy(payload->headers, headers ? headers : "", sizeof(payload->headers) - 1);
    strncpy(payload->body, body ? body : "", sizeof(payload->body) - 1);
}

void protocol_process_server_request(L7_Payload *payload) {
    if (!payload) return;
    
    switch (payload->protocol) {
        case PROTO_HTTP: {
            char m[32] = {0};
            const char *src = payload->method_or_cmd;
            while (*src == ' ' || *src == '\t') src++;
            strncpy(m, src, sizeof(m) - 1);
            if (m[0] == '\0') strcpy(m, "GET");

            if (strncasecmp(m, "GET", 3) == 0) {
                if (strcmp(payload->target, "/") == 0 || strcmp(payload->target, "/index.html") == 0) {
                    payload->status_code = 200;
                    strncpy(payload->status_message, "OK", sizeof(payload->status_message) - 1);
                    snprintf(payload->response_body, sizeof(payload->response_body),
                             "<!DOCTYPE html>\n<html>\n<head><title>Simulated Web Server</title></head>\n"
                             "<body>\n<h1>200 OK</h1>\n<p>Welcome! Content served across the simulated TCP/IP stack.</p>\n</body>\n</html>");
                } else if (strcmp(payload->target, "/api/data") == 0) {
                    payload->status_code = 200;
                    strncpy(payload->status_message, "OK", sizeof(payload->status_message) - 1);
                    snprintf(payload->response_body, sizeof(payload->response_body),
                             "{\"status\": \"success\", \"service\": \"TCP-IP-Sim\", \"items\": [10, 20, 30]}");
                } else {
                    payload->status_code = 404;
                    strncpy(payload->status_message, "Not Found", sizeof(payload->status_message) - 1);
                    snprintf(payload->response_body, sizeof(payload->response_body),
                             "<!DOCTYPE html>\n<html><body><h1>404 Not Found</h1><p>Path '%s' does not exist on this server.</p></body></html>",
                             payload->target);
                }
            } else if (strncasecmp(m, "HEAD", 4) == 0) {
                if (strcmp(payload->target, "/") == 0 || strcmp(payload->target, "/index.html") == 0) {
                    payload->status_code = 200;
                    strncpy(payload->status_message, "OK (Headers Only - No Body)", sizeof(payload->status_message) - 1);
                    payload->response_body[0] = '\0'; // RFC: HEAD returns headers only, empty body
                } else {
                    payload->status_code = 404;
                    strncpy(payload->status_message, "Not Found (Headers Only)", sizeof(payload->status_message) - 1);
                    payload->response_body[0] = '\0';
                }
            } else if (strncasecmp(m, "POST", 4) == 0) {
                payload->status_code = 200;
                strncpy(payload->status_message, "OK", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "{\"status\": \"created\", \"resource\": \"%s\", \"payload_size\": %zu}",
                         payload->target, strlen(payload->body));
            } else if (strncasecmp(m, "PUT", 3) == 0) {
                payload->status_code = 200;
                strncpy(payload->status_message, "200 OK (Resource Updated)", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "{\"status\": \"updated\", \"resource\": \"%s\", \"bytes_written\": %zu, \"etag\": \"\\\"e03a-129b\\\"\"}",
                         payload->target, strlen(payload->body));
            } else {
                payload->status_code = 405;
                strncpy(payload->status_message, "Method Not Allowed", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body), "Method '%s' not supported.", m);
            }
            break;
        }
        
        case PROTO_DNS: {
            payload->status_code = 0; // DNS RCODE 0 = No Error
            strncpy(payload->status_message, "NOERROR", sizeof(payload->status_message) - 1);
            
            char resolved_ip[32] = "93.184.216.34"; // Default example.com
            if (strstr(payload->target, "google")) {
                strcpy(resolved_ip, "142.250.190.46");
            } else if (strstr(payload->target, "wikipedia")) {
                strcpy(resolved_ip, "208.80.154.224");
            } else if (strstr(payload->target, "github")) {
                strcpy(resolved_ip, "140.82.121.4");
            }
            
            snprintf(payload->response_body, sizeof(payload->response_body),
                     ";; QUESTION SECTION:\n;%s. IN A\n\n"
                     ";; ANSWER SECTION:\n%s. 300 IN A %s\n\n"
                     ";; Query time: 14 msec\n;; SERVER: 8.8.8.8#53(8.8.8.8)",
                     payload->target, payload->target, resolved_ip);
            break;
        }
        
        case PROTO_CDN: {
            payload->status_code = 200;
            // Simulate Cache Hit for common video/images, Miss for other dynamic assets
            bool is_hit = (strstr(payload->target, "video") != NULL || strstr(payload->target, "logo") != NULL || strstr(payload->target, "cached") != NULL);
            if (is_hit) {
                strncpy(payload->status_message, "200 OK (Edge Cache HIT)", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "[CDN Edge Server: Frankfurt PoP]\n"
                         "Cache-Control: public, max-age=86400\n"
                         "X-Cache: HIT from edge-node-04\n"
                         "Latency: 3ms\n\n"
                         "[Content Delivery: 2.4 MB binary stream received for '%s']",
                         payload->target);
            } else {
                strncpy(payload->status_message, "200 OK (Edge Cache MISS -> Fetched from Origin)", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "[CDN Edge Server: Frankfurt PoP]\n"
                         "Cache-Control: public, max-age=3600\n"
                         "X-Cache: MISS from edge-node-04\n"
                         "Origin-Fetch-Latency: 68ms\n\n"
                         "[Origin Storage response cached at Edge and delivered for '%s']",
                         payload->target);
            }
            break;
        }
        
        case PROTO_SMTP: {
            if (payload->mail_subprotocol == MAIL_SUB_RAW_SMTP) {
                payload->status_code = 250;
                strncpy(payload->status_message, "250 Message Accepted", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "250 2.1.0 Sender <%s> OK\n"
                         "250 2.1.5 Recipient OK\n"
                         "354 Enter mail, end with \".\" on a line by itself\n"
                         "250 2.0.0 d9a0b12c Ok: queued as 4X8mP910jHz0",
                         payload->target);
            } else if (payload->mail_subprotocol == MAIL_SUB_POP3) {
                payload->status_code = 1; // +OK
                strncpy(payload->status_message, "+OK Message follows", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "+OK 482 octets\n"
                         "From: server@simnet.org\n"
                         "To: %s\n"
                         "Subject: Delivered: Notification via TCP/IP Stack\n\n"
                         "%s\n.\n",
                         payload->target, payload->body[0] ? payload->body : "No body content");
            } else if (payload->mail_subprotocol == MAIL_SUB_IMAP) {
                payload->status_code = 1;
                strncpy(payload->status_message, "OK FETCH completed", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "* 1 FETCH (FLAGS (\\Seen) RFC822.SIZE 512 BODY[TEXT] {%zu}\n%s)\nA002 OK FETCH completed",
                         strlen(payload->body), payload->body);
            } else { // Webmail HTTP
                payload->status_code = 200;
                strncpy(payload->status_message, "OK (Webmail API)", sizeof(payload->status_message) - 1);
                snprintf(payload->response_body, sizeof(payload->response_body),
                         "{\"mailbox\": \"%s\", \"messages\": [{\"id\": 101, \"subject\": \"New Message\", \"body\": \"%s\"}]}",
                         payload->target, payload->body);
            }
            break;
        }
        
        case PROTO_P2P: {
            payload->status_code = 200;
            strncpy(payload->status_message, "200 BitTorrent Piece Ready", sizeof(payload->status_message) - 1);
            snprintf(payload->response_body, sizeof(payload->response_body),
                     "BitTorrent Protocol v1.0\n"
                     "Piece Index: %s\n"
                     "Block Offset: 0x0000\n"
                     "Block Length: 16384 bytes\n"
                     "Checksum SHA-1: 9f8a3c81e289f6b92a401b3e8\n"
                     "[Piece payload binary verification passed. Swarm peers notified.]",
                     payload->target[0] ? payload->target : "Chunk #0");
            break;
        }
    }
}
