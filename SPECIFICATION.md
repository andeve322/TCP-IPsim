# Architectural Specification: Concurrent TCP/IP Stack Simulator (C & Web UI)

Engineering and operating systems reference document for implementing a concurrent TCP/IP network simulator.

---

## 1. Overview & System Goals

The simulator models network packet transit across nodes and protocol layers using POSIX threads (`pthreads`) in C:
1. **Client (End System)**: Constructs the application request and performs descending encapsulation ($L7 \to L4 \to L3 \to L2$).
2. **Intermediate Nodes**:
   - **Repeater / Physical Medium ($L1$)**: Simulates physical signal forwarding and propagation delay.
   - **Router ($L1 \to L2 \to L3$)**: Decapsulates frame to IP, checks routing table, decrements TTL, updates next-hop MAC, and re-encapsulates.
3. **Server (End System)**: Receives frame, decapsulates ascending ($L2 \to L7$), evaluates the application request, and generates a coherent response.
4. **Web Frontend**: Interactive UI for configuring protocol requests, inspecting packet headers/payloads at each step, and animating packet propagation.

---

## 2. Concurrency Architecture (C Backend)

### 2.1 Thread Topology
Each network node runs as an independent POSIX thread:
- `client_thread`: Encapsulates outgoing requests; decapsulates incoming responses.
- `repeater_thread`: Simulates L1 physical transmission and propagation delay.
- `router_thread`: L3 routing logic and hop forwarding.
- `server_thread`: Processes application requests and produces replies.

### 2.2 IPC & Synchronization Primitives
Inter-thread communication across links uses **Bounded Thread-Safe Queues**:
- `pthread_mutex_t`: Mutual exclusion for queue operations.
- `pthread_cond_t cond_not_empty`: Signals consumer thread when a packet is enqueued.
- `pthread_cond_t cond_not_full`: Blocks producer thread when channel queue is full.

---

## 3. Protocol Data Unit (PDU) & Encapsulation

```
+--------------------------------------------------------+
| Frame L2: Dest MAC | Src MAC | EtherType               |
|  +---------------------------------------------------+ |
|  | Packet L3: Dest IP | Src IP | TTL | Protocol ID   | |
|  |  +----------------------------------------------+ | |
|  |  | Segment L4: Dest Port | Src Port | Seq/Ack   | | |
|  |  |  +-----------------------------------------+ | | |
|  |  |  | Message L7: Protocol Header + Payload   | | | |
|  |  |  +-----------------------------------------+ | | |
|  |  +----------------------------------------------+ | |
|  +---------------------------------------------------+ |
+--------------------------------------------------------+
```

### Layer Definitions:
- **L7 (Application)**: Protocol identifier (`HTTP`, `DNS`, `CDN`, `SMTP`, `P2P`), headers, body.
- **L4 (Transport)**: Source/Destination ports (e.g. 80, 53, 25, 110, ephemeral ports), TCP/UDP flags.
- **L3 (Network)**: Source & Destination IPv4 addresses, TTL (Time-To-Live), Protocol (TCP=6, UDP=17).
- **L2 (Data Link)**: Source & Destination MAC addresses (Hop-by-hop resolution).
- **L1 (Physical)**: Raw frame transmission, propagation delay (`usleep`).

---

## 4. Application Protocols & Coherent Responses

| Protocol | Client Request Inputs | Server Processing Logic | Coherent Response |
| :--- | :--- | :--- | :--- |
| **HTTP** | Method (`GET`/`POST`), URI, Host, User-Agent, Body | Resource lookup in virtual filesystem | `200 OK` (with payload) or `404 Not Found` |
| **DNS** | Domain name query (`A`, `CNAME`) | DNS zone table lookup | Resolved IP address record |
| **CDN** | Media resource URL | Cache check: Edge hit vs Origin miss | Edge cache response or Origin redirection |
| **SMTP / Mail** | Sender, Recipient, Subject, Body; retrieval via `POP3`/`IMAP`/`Webmail` | Mailbox spooling | `250 OK` queued, subsequent retrieval response |
| **P2P** | Target chunk hash / piece index | Peer bitfield and swarm lookup | Piece data block payload |

---

## 5. Web UI & Backend Integration

```
┌──────────────────────────────────────────────┐
│             Web UI (Browser)                 │
│  - Protocol Selector & Header/Body Editor    │
│  - Animated Topology & Layer Inspector       │
└──────────────────────┬───────────────────────┘
                       │ JSON Events / HTTP / WebSocket
                       ▼
┌──────────────────────────────────────────────┐
│            C Backend Web Server              │
│  - Serves UI assets                          │
│  - Receives simulation trigger requests      │
│  - Streams step events (SSE or WebSocket)    │
└──────────────────────┬───────────────────────┘
                       │ Event Dispatcher
                       ▼
┌──────────────────────────────────────────────┐
│          POSIX Threads Network Model         │
│  [Client] -> [Repeater] -> [Router] -> [Server]
└──────────────────────────────────────────────┘
```

### Event Payload Format:
Whenever a packet moves between layers or across nodes, an event is emitted:
```json
{
  "timestamp": 1718000000,
  "node": "ROUTER",
  "layer": "L3",
  "action": "DECREMENT_TTL",
  "pdu": {
    "l3": { "src_ip": "192.168.1.10", "dest_ip": "93.184.216.34", "ttl": 63 },
    "l4": { "src_port": 54321, "dest_port": 80 },
    "l7": { "protocol": "HTTP", "method": "GET", "path": "/index.html" }
  }
}
```

---

## 6. Implementation Roadmap

1. **Step 1 - Data Structures**:
   - Define C structs for each layer header and the unified `Packet` container.
   - Implement the thread-safe circular queue with POSIX mutex and condition variables.
2. **Step 2 - Encapsulation / Decapsulation Functions**:
   - Write functions to wrap L7 into L4, L4 into L3, L3 into L2.
   - Write reverse unwrap functions.
3. **Step 3 - Node Threads**:
   - Implement worker loops for `client_thread`, `repeater_thread`, `router_thread`, `server_thread`.
4. **Step 4 - Protocol Handlers**:
   - Implement parsing and response generators for HTTP, DNS, CDN, SMTP, and P2P.
5. **Step 5 - Local Web Interface & Event Stream**:
   - Set up the frontend UI (HTML/CSS/JS) with interactive controls.
   - Connect the C simulation engine to stream real-time progression events to the UI.
