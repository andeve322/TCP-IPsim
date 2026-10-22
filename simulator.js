/**
 * simulator.js — Pure JavaScript port of the TCP/IP Stack Simulator engine.
 *
 * Replaces: protocols.c, nodes.c, event.c, queue.c (all C backend logic).
 * Produces the same JSON event array that the original /api/simulate endpoint returned,
 * so the existing app.js rendering code works without any changes to renderStep().
 *
 * Topology:  Client → Repeater1 → Router → Repeater2 → Server → (reverse) → Client
 */

'use strict';

// ─── Constants ────────────────────────────────────────────────────────────────

const CLIENT_IP       = '192.168.1.100';
const CLIENT_MAC      = 'AA:BB:CC:DD:EE:01';
const ROUTER_MAC_ETH0 = 'AA:BB:CC:DD:EE:02';   // Router — client-side interface
const ROUTER_MAC_ETH1 = '11:22:33:44:55:01';   // Router — server-side interface
const SERVER_IP       = '10.0.0.50';
const SERVER_MAC      = '11:22:33:44:55:02';

// ─── Protocol helpers ─────────────────────────────────────────────────────────

function getDefaultPort(proto, subproto) {
    switch (proto) {
        case 'HTTP':  return 80;
        case 'DNS':   return 53;
        case 'CDN':   return 443;
        case 'SMTP':
            if (subproto === 'POP3')    return 110;
            if (subproto === 'IMAP')    return 143;
            if (subproto === 'WEBMAIL') return 80;
            return 25;
        case 'P2P':   return 6881;
        default:      return 80;
    }
}

function portDesc(port) {
    switch (port) {
        case 80:   return 'HTTP (Port 80: Well-Known Web Server)';
        case 53:   return 'DNS (Port 53: Well-Known Nameserver)';
        case 443:  return 'HTTPS / CDN Edge (Port 443: TLS Secure Web)';
        case 25:   return 'SMTP (Port 25: Well-Known Mail Transfer)';
        case 110:  return 'POP3 (Port 110: Well-Known Mail Retrieval)';
        case 143:  return 'IMAP (Port 143: Well-Known Mailbox Sync)';
        case 6881: return 'BitTorrent (Port 6881: Registered Peer Wire)';
        default:
            if (port >= 49152) return 'Ephemeral Client Socket (Dynamic OS Range 49152-65535)';
            return 'Registered Service Port';
    }
}

function portServiceName(port) {
    switch (port) {
        case 80:   return 'HTTP (Web)';
        case 53:   return 'DNS (Nameserver)';
        case 443:  return 'HTTPS/CDN (Secure Web)';
        case 25:   return 'SMTP (Mail Transfer)';
        case 110:  return 'POP3 (Mail Retrieval)';
        case 143:  return 'IMAP (Mailbox Sync)';
        case 6881: return 'BitTorrent P2P';
        default:   return 'Dynamic/Ephemeral';
    }
}

function flagsDesc(transportProto, flags) {
    if (transportProto === 17) return 'UDP Datagram: Connectionless (No Flags / No Handshake)';
    if (flags & 0x02) return 'SYN [0x02]: Connection Request (Initiating 3-Way Handshake)';
    if ((flags & 0x10) && (flags & 0x08)) return 'ACK + PSH [0x18]: Acknowledged Data (Push immediately to Socket)';
    if (flags & 0x10) return 'ACK [0x10]: Acknowledgment of Received Bytes';
    if (flags & 0x01) return 'FIN [0x01]: Graceful Connection Teardown';
    return 'TCP Stream Data';
}

function transportRationale(proto) {
    switch (proto) {
        case 'HTTP':
            return 'HTTP uses TCP (Port 80/443): Requires lossless, in-order delivery of HTML and API data, backed by TCP flow control and retransmission timers.';
        case 'DNS':
            return 'DNS uses UDP (Port 53): Optimized for lightweight single-packet query and response, avoiding the 3-way handshake round-trip latency.';
        case 'CDN':
            return 'CDN uses TCP (Port 443/80): High-throughput video/asset streaming requires byte-level reliability and TCP sliding window congestion control.';
        case 'SMTP':
            return 'SMTP uses TCP (Port 25/110/143): State machine-driven email exchange requires reliable stream integrity so messages cannot arrive truncated or corrupted.';
        case 'P2P':
            return 'BitTorrent P2P uses TCP (Port 6881): Peer-to-peer chunk transfers require persistent full-duplex byte streams with accurate piece verification.';
        default:
            return 'Standard L4 Transport Layer.';
    }
}

// ─── Protocol processing (mirrors protocols.c) ────────────────────────────────

function processServerRequest(l7) {
    const proto = l7.protocol;

    if (proto === 'HTTP') {
        const m = (l7.method_or_cmd || 'GET').trim().toUpperCase();
        if (m.startsWith('GET')) {
            if (l7.target === '/' || l7.target === '/index.html') {
                l7.status_code = 200;
                l7.status_message = 'OK';
                l7.response_body = '<!DOCTYPE html>\n<html>\n<head><title>Simulated Web Server</title></head>\n<body>\n<h1>200 OK</h1>\n<p>Welcome! Content served across the simulated TCP/IP stack.</p>\n</body>\n</html>';
            } else if (l7.target === '/api/data') {
                l7.status_code = 200;
                l7.status_message = 'OK';
                l7.response_body = '{"status": "success", "service": "TCP-IP-Sim", "items": [10, 20, 30]}';
            } else {
                l7.status_code = 404;
                l7.status_message = 'Not Found';
                l7.response_body = `<!DOCTYPE html>\n<html><body><h1>404 Not Found</h1><p>Path '${l7.target}' does not exist on this server.</p></body></html>`;
            }
        } else if (m.startsWith('HEAD')) {
            if (l7.target === '/' || l7.target === '/index.html') {
                l7.status_code = 200;
                l7.status_message = 'OK (Headers Only - No Body)';
                l7.response_body = '';
            } else {
                l7.status_code = 404;
                l7.status_message = 'Not Found (Headers Only)';
                l7.response_body = '';
            }
        } else if (m.startsWith('POST')) {
            l7.status_code = 200;
            l7.status_message = 'OK';
            l7.response_body = `{"status": "created", "resource": "${l7.target}", "payload_size": ${l7.body.length}}`;
        } else if (m.startsWith('PUT')) {
            l7.status_code = 200;
            l7.status_message = '200 OK (Resource Updated)';
            l7.response_body = `{"status": "updated", "resource": "${l7.target}", "bytes_written": ${l7.body.length}, "etag": "\\"e03a-129b\\""}`;
        } else {
            l7.status_code = 405;
            l7.status_message = 'Method Not Allowed';
            l7.response_body = `Method '${m}' not supported.`;
        }

    } else if (proto === 'DNS') {
        l7.status_code = 0;
        l7.status_message = 'NOERROR';
        let resolved = '93.184.216.34';
        if (l7.target.includes('google'))    resolved = '142.250.190.46';
        else if (l7.target.includes('wikipedia')) resolved = '208.80.154.224';
        else if (l7.target.includes('github'))    resolved = '140.82.121.4';
        l7.response_body =
            `;; QUESTION SECTION:\n;${l7.target}. IN A\n\n` +
            `;; ANSWER SECTION:\n${l7.target}. 300 IN A ${resolved}\n\n` +
            `;; Query time: 14 msec\n;; SERVER: 8.8.8.8#53(8.8.8.8)`;

    } else if (proto === 'CDN') {
        l7.status_code = 200;
        const isHit = /video|logo|cached/i.test(l7.target);
        if (isHit) {
            l7.status_message = '200 OK (Edge Cache HIT)';
            l7.response_body =
                `[CDN Edge Server: Frankfurt PoP]\n` +
                `Cache-Control: public, max-age=86400\n` +
                `X-Cache: HIT from edge-node-04\n` +
                `Latency: 3ms\n\n` +
                `[Content Delivery: 2.4 MB binary stream received for '${l7.target}']`;
        } else {
            l7.status_message = '200 OK (Edge Cache MISS → Fetched from Origin)';
            l7.response_body =
                `[CDN Edge Server: Frankfurt PoP]\n` +
                `Cache-Control: public, max-age=3600\n` +
                `X-Cache: MISS from edge-node-04\n` +
                `Origin-Fetch-Latency: 68ms\n\n` +
                `[Origin Storage response cached at Edge and delivered for '${l7.target}']`;
        }

    } else if (proto === 'SMTP') {
        const sub = l7.mail_subprotocol;
        if (sub === 'SMTP' || !sub) {
            l7.status_code = 250;
            l7.status_message = '250 Message Accepted';
            l7.response_body =
                `250 2.1.0 Sender <${l7.target}> OK\n` +
                `250 2.1.5 Recipient OK\n` +
                `354 Enter mail, end with "." on a line by itself\n` +
                `250 2.0.0 d9a0b12c Ok: queued as 4X8mP910jHz0`;
        } else if (sub === 'POP3') {
            l7.status_code = 1;
            l7.status_message = '+OK Message follows';
            l7.response_body =
                `+OK 482 octets\n` +
                `From: server@simnet.org\n` +
                `To: ${l7.target}\n` +
                `Subject: Delivered: Notification via TCP/IP Stack\n\n` +
                `${l7.body || 'No body content'}\n.`;
        } else if (sub === 'IMAP') {
            l7.status_code = 1;
            l7.status_message = 'OK FETCH completed';
            l7.response_body =
                `* 1 FETCH (FLAGS (\\Seen) RFC822.SIZE 512 BODY[TEXT] {${l7.body.length}}\n${l7.body})\nA002 OK FETCH completed`;
        } else { // WEBMAIL
            l7.status_code = 200;
            l7.status_message = 'OK (Webmail API)';
            l7.response_body = `{"mailbox": "${l7.target}", "messages": [{"id": 101, "subject": "New Message", "body": "${l7.body}"}]}`;
        }

    } else if (proto === 'P2P') {
        l7.status_code = 200;
        l7.status_message = '200 BitTorrent Piece Ready';
        l7.response_body =
            `BitTorrent Protocol v1.0\n` +
            `Piece Index: ${l7.target || 'Chunk #0'}\n` +
            `Block Offset: 0x0000\n` +
            `Block Length: 16384 bytes\n` +
            `Checksum SHA-1: 9f8a3c81e289f6b92a401b3e8\n` +
            `[Piece payload binary verification passed. Swarm peers notified.]`;
    }
}

// ─── PDU snapshot helper ───────────────────────────────────────────────────────

function snapshotPDU(pkt) {
    // Returns the pdu object expected by app.js renderStep()
    const l7 = pkt.l7;
    const l4 = pkt.l4;
    const l3 = pkt.l3;
    const l2 = pkt.l2;
    return {
        l2: {
            src_mac:  l2.src_mac,
            dest_mac: l2.dest_mac,
            ethertype: l2.ethertype ? `0x${l2.ethertype.toString(16).padStart(4,'0')}` : '0x0800'
        },
        l3: {
            src_ip:  l3.src_ip,
            dest_ip: l3.dest_ip,
            ttl:     l3.ttl,
            proto:   l3.protocol
        },
        l4: {
            src_port:       l4.src_port,
            src_port_desc:  portDesc(l4.src_port),
            dest_port:      l4.dest_port,
            dest_port_desc: portDesc(l4.dest_port),
            transport_proto: l4.transport_proto === 17 ? 'UDP (Protocol 17)' : 'TCP (Protocol 6)',
            flags_desc:     flagsDesc(l4.transport_proto, l4.flags),
            rationale:      transportRationale(l7.protocol),
            seq:            l4.seq_num,
            ack:            l4.ack_num,
            flags:          l4.flags
        },
        l7: {
            protocol:      l7.protocol,
            method:        l7.method_or_cmd,
            target:        l7.target,
            headers:       l7.headers,
            body:          l7.body,
            status_code:   l7.status_code,
            status_msg:    l7.status_message,
            response_body: l7.response_body
        }
    };
}

// ─── Event builder ────────────────────────────────────────────────────────────

function makeEvent(stepId, direction, node, layer, action, details, pkt) {
    return {
        step_id:   stepId,
        direction: direction,
        node:      node,
        layer:     layer,
        action:    action,
        details:   details,
        pdu:       snapshotPDU(pkt)
    };
}

// ─── Main simulation engine ───────────────────────────────────────────────────

/**
 * runSimulation({ protocol, subprotocol, method, target, headers, body })
 * Returns an array of event objects identical in shape to the C backend JSON.
 */
function runSimulation({ protocol, subprotocol, method, target, headers, body }) {
    const events = [];
    let stepId = 1;

    function log(direction, node, layer, action, details, pkt) {
        events.push(makeEvent(stepId++, direction, node, layer, action, details, JSON.parse(JSON.stringify(pkt))));
    }

    // Build initial packet
    const dport = getDefaultPort(protocol, subprotocol);
    const isUdp = protocol === 'DNS';
    const transportProto = isUdp ? 17 : 6;

    const pkt = {
        direction: 'FORWARD',
        l2: { src_mac: '', dest_mac: '', ethertype: 0x0800 },
        l3: { src_ip: '', dest_ip: '', ttl: 0, protocol: transportProto },
        l4: {
            src_port: 52144,
            dest_port: dport,
            seq_num: 0,
            ack_num: 0,
            flags: 0,
            transport_proto: transportProto
        },
        l7: {
            protocol:          protocol,
            mail_subprotocol:  subprotocol || '',
            method_or_cmd:     method || '',
            target:            target || '',
            headers:           headers || '',
            body:              body || '',
            status_code:       0,
            status_message:    '',
            response_body:     ''
        }
    };

    // ── CLIENT (forward, descend stack L7 → L1) ───────────────────────────────

    log('FORWARD', 'CLIENT', 7,
        'L7 Composition',
        'Composed application request with method and headers',
        pkt);

    // L4 encapsulation
    pkt.l4.seq_num = 1000;
    pkt.l4.ack_num = 0;
    pkt.l4.flags   = 0x02; // SYN

    const l4Desc = isUdp
        ? `L4 UDP Datagram: Ephemeral Src Port 52144 → Well-Known Dest Port 53 (DNS). Connectionless mode for rapid lookup.`
        : `L4 TCP Segment: Ephemeral Src Port 52144 → Well-Known Dest Port ${dport} (${portServiceName(dport)}). Flags: SYN (Seq=1000) for reliable stream.`;
    log('FORWARD', 'CLIENT', 4,
        isUdp ? 'L4 UDP Encap' : 'L4 TCP Encap',
        l4Desc, pkt);

    // L3 encapsulation
    pkt.l3.src_ip  = CLIENT_IP;
    pkt.l3.dest_ip = SERVER_IP;
    pkt.l3.ttl     = 64;
    log('FORWARD', 'CLIENT', 3,
        'L3 Encapsulation',
        `Encapsulated into IPv4 packet (Src: ${CLIENT_IP}, Dest: ${SERVER_IP}, TTL: 64)`,
        pkt);

    // L2 encapsulation
    pkt.l2.src_mac  = CLIENT_MAC;
    pkt.l2.dest_mac = ROUTER_MAC_ETH0;
    pkt.l2.ethertype = 0x0800;
    log('FORWARD', 'CLIENT', 2,
        'L2 Encapsulation',
        `Encapsulated into Ethernet frame (Next-Hop MAC: ${ROUTER_MAC_ETH0})`,
        pkt);

    // L1 transmission
    log('FORWARD', 'CLIENT', 1,
        'L1 Transmission',
        'Pushed raw frame onto link toward Repeater 1',
        pkt);

    // ── REPEATER 1 (forward, L1 only) ─────────────────────────────────────────
    log('FORWARD', 'REPEATER_1', 1,
        'Signal Repeat',
        'Repeater 1 regenerated physical signal and forwarded to Router',
        pkt);

    // ── ROUTER (forward, L1 → L2 check → L3 route → L2 rewrite → L1) ─────────
    log('FORWARD', 'ROUTER', 1,
        'L1 Inbound',
        'Frame bits received on interface eth0',
        pkt);

    log('FORWARD', 'ROUTER', 2,
        'L2 Check',
        `Verified Destination MAC matches eth0 (${ROUTER_MAC_ETH0})`,
        pkt);

    pkt.l3.ttl--;
    log('FORWARD', 'ROUTER', 3,
        'L3 Routing',
        `Routing table lookup: Dest IP ${SERVER_IP} matches eth1; TTL decremented to ${pkt.l3.ttl}`,
        pkt);

    pkt.l2.src_mac  = ROUTER_MAC_ETH1;
    pkt.l2.dest_mac = SERVER_MAC;
    log('FORWARD', 'ROUTER', 2,
        'L2 Rewrite',
        `Updated Next-Hop MAC to Server (${SERVER_MAC})`,
        pkt);

    log('FORWARD', 'ROUTER', 1,
        'L1 Transmission',
        'Pushed frame bits onto wire (eth1) toward Repeater 2',
        pkt);

    // ── REPEATER 2 (forward, L1 only) ─────────────────────────────────────────
    log('FORWARD', 'REPEATER_2', 1,
        'Signal Repeat',
        'Repeater 2 regenerated physical signal and forwarded to Server',
        pkt);

    // ── SERVER (forward, ascend L1 → L7; then process; then reverse, descend L7 → L1) ──

    log('FORWARD', 'SERVER', 1,
        'L1 Reception',
        'Bits received on server network interface',
        pkt);

    log('FORWARD', 'SERVER', 2,
        'L2 Verification',
        `Ethernet MAC matched Server MAC (${SERVER_MAC}), stripped frame header`,
        pkt);

    log('FORWARD', 'SERVER', 3,
        'L3 Verification',
        `IP matched Server IP (${SERVER_IP}), protocol parsed`,
        pkt);

    const l4ServerDesc = isUdp
        ? 'UDP Datagram Demux: Dest Port 53 matched listening DNS resolver daemon (0.0.0.0:53). Checksum validated.'
        : `TCP Segment Demux: Dest Port ${dport} matched listening ${portServiceName(dport)} daemon socket (0.0.0.0:${dport}). Seq=1000 verified against socket buffer.`;
    log('FORWARD', 'SERVER', 4,
        isUdp ? 'L4 UDP Demux' : 'L4 TCP Demux',
        l4ServerDesc, pkt);

    log('FORWARD', 'SERVER', 7,
        'L7 Processing',
        'Evaluating application payload and generating coherent response',
        pkt);

    // Business logic — mutates pkt.l7 with status_code / response_body
    processServerRequest(pkt.l7);

    // Build reverse packet
    pkt.direction = 'REVERSE';
    const origSrc = pkt.l4.src_port;
    const origDst = pkt.l4.dest_port;
    pkt.l4.src_port  = origDst;
    pkt.l4.dest_port = origSrc;
    pkt.l4.seq_num   = 5000;
    pkt.l4.ack_num   = 1001;
    pkt.l4.flags     = 0x18; // ACK + PSH

    pkt.l3.src_ip  = SERVER_IP;
    pkt.l3.dest_ip = CLIENT_IP;
    pkt.l3.ttl     = 64;

    pkt.l2.src_mac  = SERVER_MAC;
    pkt.l2.dest_mac = ROUTER_MAC_ETH1;

    log('REVERSE', 'SERVER', 7,
        'L7 Response Ready',
        'Application generated status and response payload',
        pkt);

    const l4RespDesc = isUdp
        ? `UDP Response Datagram: Swapped ports (Src: ${origDst} [DNS Server] → Dest: ${origSrc} [Client socket]). Single datagram reply.`
        : `TCP Response Segment: Swapped ports (Src: ${origDst} [${portServiceName(origDst)}] → Dest: ${origSrc} [Client]). Set ACK+PSH, Seq=5000, Ack=1001.`;
    log('REVERSE', 'SERVER', 4,
        isUdp ? 'L4 UDP Response Encap' : 'L4 TCP Response Encap',
        l4RespDesc, pkt);

    log('REVERSE', 'SERVER', 3,
        'L3 Response Encap',
        `Encapsulated into IPv4 packet (Dest: ${CLIENT_IP}, TTL: 64)`,
        pkt);

    log('REVERSE', 'SERVER', 2,
        'L2 Response Encap',
        `Encapsulated into Ethernet frame (Next-Hop: ${ROUTER_MAC_ETH1})`,
        pkt);

    log('REVERSE', 'SERVER', 1,
        'L1 Transmission',
        'Pushed response frame onto wire toward Repeater 2',
        pkt);

    // ── REPEATER 2 (reverse, L1 only) ─────────────────────────────────────────
    log('REVERSE', 'REPEATER_2', 1,
        'Signal Repeat',
        'Repeater 2 regenerated response signal and forwarded to Router',
        pkt);

    // ── ROUTER (reverse) ───────────────────────────────────────────────────────
    log('REVERSE', 'ROUTER', 1,
        'L1 Inbound',
        'Response frame bits received on eth1',
        pkt);

    log('REVERSE', 'ROUTER', 2,
        'L2 Check',
        `Verified Destination MAC matches eth1 (${ROUTER_MAC_ETH1})`,
        pkt);

    pkt.l3.ttl--;
    log('REVERSE', 'ROUTER', 3,
        'L3 Routing',
        `Routing response: Dest IP ${CLIENT_IP} matches eth0; TTL decremented to ${pkt.l3.ttl}`,
        pkt);

    pkt.l2.src_mac  = ROUTER_MAC_ETH0;
    pkt.l2.dest_mac = CLIENT_MAC;
    log('REVERSE', 'ROUTER', 2,
        'L2 Rewrite',
        `Updated Next-Hop MAC to Client (${CLIENT_MAC})`,
        pkt);

    log('REVERSE', 'ROUTER', 1,
        'L1 Transmission',
        'Pushed response bits onto wire (eth0) toward Repeater 1',
        pkt);

    // ── REPEATER 1 (reverse, L1 only) ─────────────────────────────────────────
    log('REVERSE', 'REPEATER_1', 1,
        'Signal Repeat',
        'Repeater 1 regenerated response signal and forwarded to Client',
        pkt);

    // ── CLIENT (reverse, ascend L1 → L7) ─────────────────────────────────────
    log('REVERSE', 'CLIENT', 1,
        'L1 Reception',
        'Bits received on physical network interface',
        pkt);

    log('REVERSE', 'CLIENT', 2,
        'L2 Decapsulation',
        `Ethernet frame MAC verified (${CLIENT_MAC}), frame stripped`,
        pkt);

    log('REVERSE', 'CLIENT', 3,
        'L3 Decapsulation',
        'IPv4 packet destination verified, IP header stripped',
        pkt);

    log('REVERSE', 'CLIENT', 4,
        'L4 Decapsulation',
        'Transport segment matched to active connection socket (52144)',
        pkt);

    log('REVERSE', 'CLIENT', 7,
        'L7 Delivery',
        `Application received response (Code ${pkt.l7.status_code}: ${pkt.l7.status_message})`,
        pkt);

    return events;
}

// Export for use in app.js (browser global)
window.TCPIPSim = { runSimulation };
