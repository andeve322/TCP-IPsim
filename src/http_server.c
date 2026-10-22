#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "http_server.h"

#define BUFFER_SIZE 65536

void http_server_init(HttpServer *server, int port, NetworkTopology *topology) {
    if (!server) return;
    server->port = port;
    server->server_fd = -1;
    server->running = false;
    server->topology = topology;
}

// Simple JSON string value extractor
static void extract_json_str(const char *json, const char *key, char *output, size_t max_len) {
    output[0] = '\0';
    if (!json || !key) return;
    
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char *pos = strstr(json, pattern);
    if (!pos) {
        snprintf(pattern, sizeof(pattern), "\"%s\" :", key);
        pos = strstr(json, pattern);
        if (!pos) return;
    }
    
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    
    if (*pos != '"') return;
    pos++; // Skip opening quote
    
    size_t i = 0;
    while (*pos && i + 1 < max_len) {
        if (*pos == '\\' && *(pos + 1)) {
            pos++;
            if (*pos == 'n') output[i++] = '\n';
            else if (*pos == 'r') output[i++] = '\r';
            else if (*pos == 't') output[i++] = '\t';
            else if (*pos == '"') output[i++] = '"';
            else if (*pos == '\\') output[i++] = '\\';
            else output[i++] = *pos;
        } else if (*pos == '"') {
            break; // Closing unescaped quote
        } else {
            output[i++] = *pos;
        }
        pos++;
    }
    output[i] = '\0';
}

static void serve_static_file(int client_fd, const char *path, const char *content_type) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
        send(client_fd, not_found, strlen(not_found), 0);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *body = malloc(fsize + 1);
    if (!body) {
        fclose(f);
        return;
    }
    fread(body, 1, fsize, f);
    fclose(f);
    
    char header[512];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %ld\r\n"
                        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                        "Connection: close\r\n\r\n",
                        content_type, fsize);
    
    send(client_fd, header, hlen, 0);
    send(client_fd, body, fsize, 0);
    free(body);
}

static void handle_client(HttpServer *server, int client_fd) {
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        close(client_fd);
        return;
    }
    
    ssize_t total_bytes = 0;
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        free(buffer);
        close(client_fd);
        return;
    }
    total_bytes = bytes_read;
    buffer[total_bytes] = '\0';
    
    // Parse Request Line
    char method[16], uri[256];
    if (sscanf(buffer, "%15s %255s", method, uri) != 2) {
        free(buffer);
        close(client_fd);
        return;
    }
    
    if (strcasecmp(method, "OPTIONS") == 0) {
        const char *cors_resp = "HTTP/1.1 204 No Content\r\n"
                                "Access-Control-Allow-Origin: *\r\n"
                                "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                "Access-Control-Allow-Headers: Content-Type\r\n"
                                "Connection: close\r\n\r\n";
        send(client_fd, cors_resp, strlen(cors_resp), 0);
        free(buffer);
        close(client_fd);
        return;
    }
    
    if (strcasecmp(method, "GET") == 0) {
        if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0) {
            serve_static_file(client_fd, "static/index.html", "text/html; charset=utf-8");
        } else if (strcmp(uri, "/style.css") == 0) {
            serve_static_file(client_fd, "static/style.css", "text/css; charset=utf-8");
        } else if (strcmp(uri, "/app.js") == 0) {
            serve_static_file(client_fd, "static/app.js", "application/javascript; charset=utf-8");
        } else {
            const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
            send(client_fd, not_found, strlen(not_found), 0);
        }
    } else if (strcasecmp(method, "POST") == 0 && strcmp(uri, "/api/simulate") == 0) {
        // Find header end
        char *body_pos = strstr(buffer, "\r\n\r\n");
        while (!body_pos && total_bytes < BUFFER_SIZE - 1) {
            ssize_t n = read(client_fd, buffer + total_bytes, (BUFFER_SIZE - 1) - total_bytes);
            if (n <= 0) break;
            total_bytes += n;
            buffer[total_bytes] = '\0';
            body_pos = strstr(buffer, "\r\n\r\n");
        }
        
        int content_len = 0;
        char *cl_pos = strcasestr(buffer, "Content-Length:");
        if (cl_pos) {
            content_len = atoi(cl_pos + 15);
        }
        
        if (body_pos) {
            body_pos += 4;
            ssize_t current_body_len = (buffer + total_bytes) - body_pos;
            while (current_body_len < content_len && total_bytes < BUFFER_SIZE - 1) {
                ssize_t n = read(client_fd, buffer + total_bytes, (BUFFER_SIZE - 1) - total_bytes);
                if (n <= 0) break;
                total_bytes += n;
                buffer[total_bytes] = '\0';
                current_body_len = (buffer + total_bytes) - body_pos;
            }
        } else {
            body_pos = "";
        }
        
        char proto_str[32] = "HTTP";
        char subproto_str[32] = "SMTP";
        char cmd[32] = "GET";
        char target[MAX_PATH_LEN] = "/index.html";
        char headers[MAX_HEADER_LEN] = "Host: example.com";
        char payload_body[MAX_BODY_LEN] = "";
        
        extract_json_str(body_pos, "protocol", proto_str, sizeof(proto_str));
        extract_json_str(body_pos, "subprotocol", subproto_str, sizeof(subproto_str));
        extract_json_str(body_pos, "method", cmd, sizeof(cmd));
        extract_json_str(body_pos, "target", target, sizeof(target));
        extract_json_str(body_pos, "headers", headers, sizeof(headers));
        extract_json_str(body_pos, "body", payload_body, sizeof(payload_body));
        
        AppProtocol proto = PROTO_HTTP;
        if (strcasecmp(proto_str, "DNS") == 0) proto = PROTO_DNS;
        else if (strcasecmp(proto_str, "CDN") == 0) proto = PROTO_CDN;
        else if (strcasecmp(proto_str, "SMTP") == 0) proto = PROTO_SMTP;
        else if (strcasecmp(proto_str, "P2P") == 0) proto = PROTO_P2P;
        
        MailSubProtocol subproto = MAIL_SUB_RAW_SMTP;
        if (strcasecmp(subproto_str, "POP3") == 0) subproto = MAIL_SUB_POP3;
        else if (strcasecmp(subproto_str, "IMAP") == 0) subproto = MAIL_SUB_IMAP;
        else if (strcasecmp(subproto_str, "WEBMAIL") == 0) subproto = MAIL_SUB_WEBMAIL;
        
        // Execute simulation across threads
        char *json_events = topology_run_simulation(server->topology, proto, subproto, cmd, target, headers, payload_body);
        
        size_t json_len = strlen(json_events);
        char resp_hdr[512];
        int hlen = snprintf(resp_hdr, sizeof(resp_hdr),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Access-Control-Allow-Origin: *\r\n"
                            "Content-Length: %zu\r\n"
                            "Connection: close\r\n\r\n",
                            json_len);
        
        send(client_fd, resp_hdr, hlen, 0);
        send(client_fd, json_events, json_len, 0);
        free(json_events);
    } else {
        const char *bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
        send(client_fd, bad_req, strlen(bad_req), 0);
    }
    
    free(buffer);
    close(client_fd);
}

void http_server_start(HttpServer *server) {
    if (!server) return;
    
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        perror("Socket creation failed");
        return;
    }
    
    int opt = 1;
    setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server->port);
    
    if (bind(server->server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server->server_fd);
        return;
    }
    
    if (listen(server->server_fd, 10) < 0) {
        perror("Listen failed");
        close(server->server_fd);
        return;
    }
    
    server->running = true;
    printf("\n=======================================================\n");
    printf(" [TCP/IP Stack Simulator Backend Running]\n");
    printf(" Open Web UI at: http://localhost:%d\n", server->port);
    printf("=======================================================\n\n");
    
    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!server->running) break;
            continue;
        }
        handle_client(server, client_fd);
    }
}

void http_server_stop(HttpServer *server) {
    if (!server) return;
    server->running = false;
    if (server->server_fd >= 0) {
        close(server->server_fd);
        server->server_fd = -1;
    }
}
