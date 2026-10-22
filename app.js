document.addEventListener('DOMContentLoaded', () => {
    // Elements
    const protoSelect = document.getElementById('protoSelect');
    const subprotoGroup = document.getElementById('subprotoGroup');
    const subprotoSelect = document.getElementById('subprotoSelect');
    const methodSelect = document.getElementById('methodSelect');
    const methodInput = document.getElementById('methodInput');
    const targetInput = document.getElementById('targetInput');
    const headerPresetSelect = document.getElementById('headerPresetSelect');
    const headersInput = document.getElementById('headersInput');
    const bodyInput = document.getElementById('bodyInput');
    const startSimBtn = document.getElementById('startSimBtn');

    // Playback
    const btnPrev = document.getElementById('btnPrev');
    const btnPlayPause = document.getElementById('btnPlayPause');
    const btnNext = document.getElementById('btnNext');
    const btnReset = document.getElementById('btnReset');
    const currentStepText = document.getElementById('currentStepText');
    const stepProgressBar = document.getElementById('stepProgressBar');
    const speedSlider = document.getElementById('speedSlider');
    const speedVal = document.getElementById('speedVal');

    // Banner & Nodes
    const bannerTitle = document.getElementById('bannerTitle');
    const bannerDetails = document.getElementById('bannerDetails');
    const bannerNode = document.getElementById('bannerNode');
    const bannerLayer = document.getElementById('bannerLayer');
    const bannerDirIcon = document.getElementById('bannerDirIcon');
    const animPacket = document.getElementById('animPacket');

    // Inspector
    const inspectorDirection = document.getElementById('inspectorDirection');
    const l2SrcMac = document.getElementById('l2SrcMac');
    const l2DestMac = document.getElementById('l2DestMac');
    const l2Ether = document.getElementById('l2Ether');
    const l3SrcIp = document.getElementById('l3SrcIp');
    const l3DestIp = document.getElementById('l3DestIp');
    const l3Ttl = document.getElementById('l3Ttl');
    const l3Proto = document.getElementById('l3Proto');
    const l4SrcPort = document.getElementById('l4SrcPort');
    const l4SrcDesc = document.getElementById('l4SrcDesc');
    const l4DestPort = document.getElementById('l4DestPort');
    const l4DestDesc = document.getElementById('l4DestDesc');
    const l4ProtoBadge = document.getElementById('l4ProtoBadge');
    const l4Flags = document.getElementById('l4Flags');
    const l4Seq = document.getElementById('l4Seq');
    const l4Ack = document.getElementById('l4Ack');
    const l4Rationale = document.getElementById('l4Rationale');
    const l7Proto = document.getElementById('l7Proto');
    const l7Cmd = document.getElementById('l7Cmd');
    const l7Target = document.getElementById('l7Target');
    const l7Payload = document.getElementById('l7Payload');

    // Response
    const respStatusBadge = document.getElementById('respStatusBadge');
    const respStatusCode = document.getElementById('respStatusCode');
    const respStatusMsg = document.getElementById('respStatusMsg');
    const respBodyBox = document.getElementById('respBodyBox');
    const logStream = document.getElementById('logStream');

    // Presets
    const presetChips = document.querySelectorAll('.chip[data-preset]');

    // Simulation State
    let timeline = [];
    let currentStep = 0;
    let isPlaying = false;
    let playTimer = null;
    let stepDelay = 800;

    // Header Templates Map
    const headerTemplates = {
        HTTP: {
            GET: {
                standard: 'Host: example.com\nUser-Agent: Mozilla/5.0 (Macintosh; Apple Silicon)\nAccept: text/html,application/xhtml+xml\nAccept-Language: en-US,en;q=0.9',
                conditional: 'Host: example.com\nUser-Agent: SimBrowser/1.0\nIf-Modified-Since: Wed, 21 Oct 2026 07:28:00 GMT\nIf-None-Match: "e03a-129b"',
                auth: 'Host: example.com\nUser-Agent: SimBrowser/1.0\nAuthorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.simulated_token\nAccept: application/json'
            },
            POST: {
                standard: 'Host: example.com\nUser-Agent: SimBrowser/1.0\nContent-Type: application/json\nContent-Length: 48\nAccept: application/json',
                conditional: 'Host: example.com\nContent-Type: application/x-www-form-urlencoded\nIf-Match: "e03a-129b"\nCache-Control: no-cache',
                auth: 'Host: example.com\nAuthorization: Bearer secure_api_key_8492\nContent-Type: application/json\nAccept: application/json'
            },
            PUT: {
                standard: 'Host: example.com\nUser-Agent: SimBrowser/1.0\nContent-Type: application/json\nIf-Match: "e03a-129b"\nContent-Length: 64',
                conditional: 'Host: example.com\nContent-Type: text/plain\nIf-Unmodified-Since: Wed, 21 Oct 2026 07:00:00 GMT\nContent-Length: 32',
                auth: 'Host: example.com\nAuthorization: Bearer admin_token_99182\nContent-Type: application/json\nIf-Match: "e03a-129b"'
            },
            HEAD: {
                standard: 'Host: example.com\nUser-Agent: SimBrowser/1.0 (HeadCheck)\nAccept: */*\nConnection: keep-alive',
                conditional: 'Host: example.com\nIf-None-Match: "e03a-129b"\nCache-Control: max-age=0',
                auth: 'Host: example.com\nAuthorization: Bearer eyJhbGciOiJIUzI1NiI.head_auth\nAccept: */*'
            }
        },
        DNS: {
            QUERY: {
                standard: 'Query-Class: IN (Internet)\nQuery-Type: A (IPv4 Host)\nRecursion-Desired: 1',
                conditional: 'Query-Class: IN\nQuery-Type: CNAME\nEDNS0-UDP-Size: 4096\nDNSSEC: Enabled (DO=1)',
                auth: 'TSIG-Key: hmac-sha256:client-key\nQuery-Type: A\nRecursion-Desired: 1'
            }
        },
        CDN: {
            GET: {
                standard: 'Host: cdn-edge.fastnet.com\nUser-Agent: SimVideoPlayer/2.0\nAccept: video/mp4,video/*\nRange: bytes=0-1048576',
                conditional: 'Host: cdn-edge.fastnet.com\nIf-None-Match: "cdn-v2-491a"\nCache-Control: max-age=86400',
                auth: 'Host: cdn-edge.fastnet.com\nCDN-Token: hmac_exp_17300000_sig99281\nRange: bytes=0-1048576'
            }
        },
        SMTP: {
            MAIL: {
                standard: 'From: alice@simnet.org\nTo: bob@server.org\nSubject: Exam Study Meeting\nMIME-Version: 1.0\nContent-Type: text/plain; charset=utf-8',
                conditional: 'From: alice@simnet.org\nTo: bob@server.org\nSubject: Urgent: OS Project Submission\nPriority: Urgent\nX-Mailer: SimMail/1.0',
                auth: 'From: alice@simnet.org\nAUTH: PLAIN AGd1ZXN0AG15cGFzcw==\nTo: bob@server.org\nSubject: Authenticated Submission'
            }
        },
        P2P: {
            PIECE: {
                standard: 'Protocol-ID: BitTorrent protocol\nInfo-Hash: 4a3f2b1c8e9d00124a9918237461524312\nPeer-ID: -TR2940-109283019283\nBitfield: 11110001',
                conditional: 'Protocol-ID: BitTorrent protocol\nExtension: ut_metadata (ID 2)\nPeer-ID: -TR2940-109283019283\nRequest: Piece #42',
                auth: 'Protocol-ID: BitTorrent protocol\nSwarm-Token: bittorrent-dht-node-8891\nPeer-ID: -TR2940-109283019283'
            }
        }
    };

    // Populate Method Select based on Protocol
    function updateMethodOptions() {
        const proto = protoSelect.value;
        methodSelect.innerHTML = '';

        if (proto === 'HTTP') {
            methodSelect.innerHTML = `
                <option value="GET">GET (Retrieve resource)</option>
                <option value="POST">POST (Submit payload)</option>
                <option value="PUT">PUT (Replace/Store resource)</option>
                <option value="HEAD">HEAD (Fetch headers only - no body)</option>
            `;
        } else if (proto === 'DNS') {
            methodSelect.innerHTML = `
                <option value="QUERY">QUERY (DNS Lookup)</option>
            `;
        } else if (proto === 'CDN') {
            methodSelect.innerHTML = `
                <option value="GET">GET (Edge Stream)</option>
            `;
        } else if (proto === 'SMTP') {
            const sub = subprotoSelect.value;
            if (sub === 'POP3') {
                methodSelect.innerHTML = `<option value="RETR 1">RETR 1 (Retrieve Message)</option>`;
            } else if (sub === 'IMAP') {
                methodSelect.innerHTML = `<option value="FETCH 1">FETCH 1 (Sync Message)</option>`;
            } else if (sub === 'WEBMAIL') {
                methodSelect.innerHTML = `<option value="GET">GET (/mail/inbox)</option>`;
            } else {
                methodSelect.innerHTML = `<option value="MAIL FROM">MAIL FROM (SMTP Delivery)</option>`;
            }
        } else if (proto === 'P2P') {
            methodSelect.innerHTML = `
                <option value="REQUEST_CHUNK">REQUEST_CHUNK (Download Piece)</option>
            `;
        }

        methodInput.value = methodSelect.value;
        updateHeaderPresetOptions();
        updateCoherentHeaders();
        updateCoherentBody();
    }

    // Populate Header Presets Dropdown
    function updateHeaderPresetOptions() {
        const proto = protoSelect.value;
        const method = methodSelect.value;

        headerPresetSelect.innerHTML = '';
        if (proto === 'HTTP') {
            headerPresetSelect.innerHTML = `
                <option value="standard">Standard Web Browser Headers</option>
                <option value="conditional">Conditional Caching (${method === 'HEAD' || method === 'GET' ? 'If-None-Match' : 'If-Match'})</option>
                <option value="auth">Authenticated API (Bearer Token)</option>
                <option value="custom">Custom (Keep Manual Edits)</option>
            `;
        } else if (proto === 'DNS') {
            headerPresetSelect.innerHTML = `
                <option value="standard">Standard Recursive Query</option>
                <option value="conditional">DNSSEC Enabled Query</option>
                <option value="auth">Authenticated TSIG Query</option>
                <option value="custom">Custom (Keep Manual Edits)</option>
            `;
        } else if (proto === 'CDN') {
            headerPresetSelect.innerHTML = `
                <option value="standard">Standard Edge Range Request</option>
                <option value="conditional">Cache Revalidation Headers</option>
                <option value="auth">Token-Protected Edge Stream</option>
                <option value="custom">Custom (Keep Manual Edits)</option>
            `;
        } else if (proto === 'SMTP') {
            headerPresetSelect.innerHTML = `
                <option value="standard">Standard RFC 5322 Envelope</option>
                <option value="conditional">High Priority Notification</option>
                <option value="auth">Authenticated SMTP (AUTH PLAIN)</option>
                <option value="custom">Custom (Keep Manual Edits)</option>
            `;
        } else if (proto === 'P2P') {
            headerPresetSelect.innerHTML = `
                <option value="standard">Standard BitTorrent Handshake</option>
                <option value="conditional">Extended Metadata Protocol</option>
                <option value="auth">DHT Swarm Authenticated</option>
                <option value="custom">Custom (Keep Manual Edits)</option>
            `;
        }
    }

    // Generate coherent headers based on current protocol & method
    function updateCoherentHeaders() {
        const preset = headerPresetSelect.value;
        if (preset === 'custom') return; // Do not overwrite user's manual changes

        const proto = protoSelect.value;
        const method = methodSelect.value;

        if (proto === 'HTTP') {
            const m = (method === 'HEAD' || method === 'POST' || method === 'PUT') ? method : 'GET';
            const group = headerTemplates.HTTP[m];
            if (group && group[preset]) {
                headersInput.value = group[preset];
                return;
            }
        } else if (proto === 'DNS') {
            if (headerTemplates.DNS.QUERY[preset]) {
                headersInput.value = headerTemplates.DNS.QUERY[preset];
                return;
            }
        } else if (proto === 'CDN') {
            if (headerTemplates.CDN.GET[preset]) {
                headersInput.value = headerTemplates.CDN.GET[preset];
                return;
            }
        } else if (proto === 'SMTP') {
            if (headerTemplates.SMTP.MAIL[preset]) {
                headersInput.value = headerTemplates.SMTP.MAIL[preset];
                return;
            }
        } else if (proto === 'P2P') {
            if (headerTemplates.P2P.PIECE[preset]) {
                headersInput.value = headerTemplates.P2P.PIECE[preset];
                return;
            }
        }

        // Fallback default
        headersInput.value = 'Host: example.com\nUser-Agent: SimBrowser/1.0';
    }

    // Generate coherent body based on HTTP method or protocol
    function updateCoherentBody() {
        const proto = protoSelect.value;
        const method = methodSelect.value;

        if (proto === 'HTTP') {
            if (method === 'GET' || method === 'HEAD') {
                bodyInput.value = '';
                bodyInput.placeholder = `${method} requests do not transmit a request body.`;
            } else if (method === 'POST') {
                bodyInput.placeholder = 'JSON or Form payload';
                if (!bodyInput.value || bodyInput.value.includes('Updated content')) {
                    bodyInput.value = '{\n  "action": "submit",\n  "user_id": 1042,\n  "status": "active"\n}';
                }
            } else if (method === 'PUT') {
                bodyInput.placeholder = 'Replacement resource content';
                if (!bodyInput.value || bodyInput.value.includes('user_id')) {
                    bodyInput.value = '{\n  "resource": "/index.html",\n  "version": 2.1,\n  "content": "Updated content via HTTP PUT"\n}';
                }
            }
        } else if (proto === 'SMTP') {
            bodyInput.placeholder = 'Message body text';
            if (!bodyInput.value) {
                bodyInput.value = 'Hello Bob,\nThis is a simulated message sent through the multi-threaded TCP/IP stack.\nRegards,\nAlice';
            }
        } else if (proto === 'P2P') {
            bodyInput.placeholder = 'Block offset and length';
            bodyInput.value = 'Block offset: 0x0000, Length: 16384 bytes';
        } else {
            bodyInput.value = '';
            bodyInput.placeholder = 'Optional payload data';
        }
    }

    // Event Listeners for controls
    protoSelect.addEventListener('change', () => {
        subprotoGroup.style.display = (protoSelect.value === 'SMTP') ? 'flex' : 'none';
        if (protoSelect.value === 'HTTP') {
            targetInput.value = '/index.html';
        } else if (protoSelect.value === 'DNS') {
            targetInput.value = 'google.com';
        } else if (protoSelect.value === 'CDN') {
            targetInput.value = '/media/video_hd.mp4';
        } else if (protoSelect.value === 'SMTP') {
            targetInput.value = 'alice@simnet.org';
        } else if (protoSelect.value === 'P2P') {
            targetInput.value = 'Piece #42 (Hash: 9f8a3c)';
        }
        updateMethodOptions();
    });

    subprotoSelect.addEventListener('change', () => {
        const sub = subprotoSelect.value;
        if (sub === 'POP3') {
            targetInput.value = 'alice@simnet.org';
        } else if (sub === 'IMAP') {
            targetInput.value = 'INBOX';
        } else if (sub === 'WEBMAIL') {
            targetInput.value = '/mail/inbox';
        } else {
            targetInput.value = 'alice@simnet.org';
        }
        updateMethodOptions();
    });

    methodSelect.addEventListener('change', () => {
        methodInput.value = methodSelect.value;
        updateHeaderPresetOptions();
        updateCoherentHeaders();
        updateCoherentBody();
    });

    headerPresetSelect.addEventListener('change', () => {
        updateCoherentHeaders();
    });

    headersInput.addEventListener('input', () => {
        // If user manually modifies headers, reflect custom in dropdown
        headerPresetSelect.value = 'custom';
    });

    // Preset Chips
    presetChips.forEach(chip => {
        chip.addEventListener('click', () => {
            presetChips.forEach(c => c.classList.remove('active'));
            chip.classList.add('active');
            const key = chip.getAttribute('data-preset');
            applyPreset(key);
        });
    });

    function applyPreset(key) {
        if (key === 'http-get') {
            protoSelect.value = 'HTTP';
            subprotoGroup.style.display = 'none';
            targetInput.value = '/index.html';
            updateMethodOptions();
            methodSelect.value = 'GET';
            methodInput.value = 'GET';
        } else if (key === 'dns-query') {
            protoSelect.value = 'DNS';
            subprotoGroup.style.display = 'none';
            targetInput.value = 'google.com';
            updateMethodOptions();
        } else if (key === 'cdn-hit') {
            protoSelect.value = 'CDN';
            subprotoGroup.style.display = 'none';
            targetInput.value = '/media/video_hd.mp4';
            updateMethodOptions();
        } else if (key === 'smtp-send') {
            protoSelect.value = 'SMTP';
            subprotoGroup.style.display = 'flex';
            subprotoSelect.value = 'SMTP';
            targetInput.value = 'alice@simnet.org';
            updateMethodOptions();
        } else if (key === 'p2p-piece') {
            protoSelect.value = 'P2P';
            subprotoGroup.style.display = 'none';
            targetInput.value = 'Piece #42 (Hash: 9f8a3c)';
            updateMethodOptions();
        }
        updateHeaderPresetOptions();
        updateCoherentHeaders();
        updateCoherentBody();
    }

    // Speed Slider
    speedSlider.addEventListener('input', (e) => {
        stepDelay = parseInt(e.target.value, 10);
        speedVal.textContent = (stepDelay / 1000).toFixed(1) + 's';
        if (isPlaying) {
            clearInterval(playTimer);
            playTimer = setInterval(stepForward, stepDelay);
        }
    });

    // Start Simulation Button
    startSimBtn.addEventListener('click', async () => {
        pausePlayback();
        startSimBtn.disabled = true;
        startSimBtn.innerHTML = 'Simulating...';

        let selMethod = methodSelect.value || methodInput.value || 'GET';
        if (protoSelect.value === 'HTTP' && selMethod.includes(' ')) {
            selMethod = selMethod.split(' ')[0].trim();
        }

        const payload = {
            protocol: protoSelect.value,
            subprotocol: subprotoSelect.value,
            method: selMethod,
            target: targetInput.value,
            headers: headersInput.value,
            body: bodyInput.value
        };

        try {
            // Pure-JS simulation engine — no server required (GitHub Pages compatible)
            timeline = window.TCPIPSim.runSimulation({
                protocol:    payload.protocol,
                subprotocol: payload.subprotocol,
                method:      payload.method,
                target:      payload.target,
                headers:     payload.headers,
                body:        payload.body
            });

            currentStep = 0;
            renderLogStream();
            btnPrev.disabled = true;
            btnNext.disabled = timeline.length <= 1;
            btnPlayPause.disabled = false;

            startSimBtn.disabled = false;
            startSimBtn.innerHTML = `
                <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2">
                    <polygon points="5 3 19 12 5 21 5 3"></polygon>
                </svg> Run Simulation`;

            // Auto-play the timeline
            renderStep(0);
            startPlayback();
        } catch (err) {
            alert('Error running simulation: ' + err.message);
            startSimBtn.disabled = false;
            startSimBtn.textContent = 'Run Simulation';
        }
    });

    // Node & Layer Coordinate Maps
    const nodeX = {
        'CLIENT': '10%',
        'REPEATER_1': '30%',
        'ROUTER': '50%',
        'REPEATER_2': '70%',
        'SERVER': '90%'
    };

    const layerY = {
        7: '34%',
        4: '42%',
        3: '50%',
        2: '58%',
        1: '66%'
    };

    function renderStep(idx) {
        if (!timeline || timeline.length === 0 || idx < 0 || idx >= timeline.length) return;
        currentStep = idx;

        const ev = timeline[idx];
        const pdu = ev.pdu;

        // Update Counter & Progress
        currentStepText.textContent = `Step ${idx + 1} / ${timeline.length}`;
        const pct = ((idx + 1) / timeline.length) * 100;
        stepProgressBar.style.width = pct + '%';

        // Update Banner
        bannerTitle.textContent = ev.action;
        bannerDetails.textContent = ev.details;
        bannerNode.textContent = ev.node;
        bannerLayer.textContent = 'Layer ' + ev.layer;
        bannerDirIcon.textContent = (ev.direction === 'FORWARD') ? '→' : '←';
        bannerDirIcon.style.color = (ev.direction === 'FORWARD') ? 'var(--primary)' : 'var(--warning)';

        // Position Animated Packet
        animPacket.style.display = 'block';
        const cx = nodeX[ev.node] || '50%';
        const cy = layerY[ev.layer] || '50%';
        animPacket.setAttribute('cx', cx);
        animPacket.setAttribute('cy', cy);
        animPacket.style.fill = (ev.direction === 'FORWARD') ? 'var(--primary)' : 'var(--warning)';

        // Update Node & Layer Pills active states
        document.querySelectorAll('.node-box').forEach(b => b.classList.remove('active-node'));
        const activeNodeBox = document.getElementById('node-' + ev.node);
        if (activeNodeBox) activeNodeBox.classList.add('active-node');

        document.querySelectorAll('.layer-pill').forEach(p => p.classList.remove('active'));
        if (activeNodeBox) {
            const activePill = activeNodeBox.querySelector(`.layer-pill[data-layer="${ev.layer}"]`);
            if (activePill) activePill.classList.add('active');
        }

        // Inspector Updates
        inspectorDirection.textContent = (ev.direction === 'FORWARD') ? 'Request (Client → Server)' : 'Response (Server → Client)';
        inspectorDirection.style.color = (ev.direction === 'FORWARD') ? 'var(--primary)' : 'var(--warning)';

        // L2
        l2SrcMac.textContent = pdu.l2.src_mac || '--';
        l2DestMac.textContent = pdu.l2.dest_mac || '--';
        l2Ether.textContent = pdu.l2.ethertype || '0x0800';

        // L3
        l3SrcIp.textContent = pdu.l3.src_ip || '--';
        l3DestIp.textContent = pdu.l3.dest_ip || '--';
        l3Ttl.textContent = pdu.l3.ttl !== undefined ? pdu.l3.ttl : '--';
        l3Proto.textContent = pdu.l3.proto === 6 ? 'TCP (6)' : (pdu.l3.proto === 17 ? 'UDP (17)' : '--');

        // L4
        const isUdp = pdu.l4.transport_proto && pdu.l4.transport_proto.includes('UDP');
        l4ProtoBadge.textContent = pdu.l4.transport_proto || (pdu.l3.proto === 17 ? 'UDP (17)' : 'TCP (6)');
        l4ProtoBadge.className = isUdp ? 'badge warning' : 'badge secondary';

        l4SrcPort.textContent = pdu.l4.src_port || '--';
        l4SrcDesc.textContent = pdu.l4.src_port_desc || 'Ephemeral Client Socket (Dynamic OS Range 49152-65535)';
        
        l4DestPort.textContent = pdu.l4.dest_port || '--';
        l4DestDesc.textContent = pdu.l4.dest_port_desc || 'Well-Known Server Listening Port';

        l4Flags.textContent = pdu.l4.flags_desc || (isUdp ? 'Connectionless Datagram' : 'TCP Connection Stream');
        l4Seq.textContent = pdu.l4.seq !== undefined ? pdu.l4.seq : '--';
        l4Ack.textContent = pdu.l4.ack !== undefined ? pdu.l4.ack : '--';

        l4Rationale.textContent = pdu.l4.rationale || 'Transport layer coordinates connection delivery, socket multiplexing, and reliability.';

        // L7
        l7Proto.textContent = pdu.l7.protocol || '--';
        l7Cmd.textContent = pdu.l7.method || '--';
        l7Target.textContent = pdu.l7.target || '--';
        l7Payload.textContent = pdu.l7.body ? pdu.l7.body : '(Empty payload)';

        // Response View
        if (pdu.l7.status_code || pdu.l7.status_msg) {
            respStatusBadge.textContent = 'Received';
            respStatusCode.textContent = pdu.l7.status_code;
            respStatusMsg.textContent = pdu.l7.status_msg;
            if (pdu.l7.method === 'HEAD') {
                respBodyBox.textContent = '// HTTP HEAD Response: Headers transmitted successfully.\n// (Notice: RFC 7231 specifies that HEAD responses MUST NOT return a body!)';
            } else {
                respBodyBox.textContent = pdu.l7.response_body || '(No response body)';
            }
        } else {
            respStatusBadge.textContent = 'Waiting for Server';
            respStatusCode.textContent = '---';
            respStatusMsg.textContent = '---';
            respBodyBox.textContent = '// Server thread has not processed this request yet...';
        }

        // Highlight log item
        highlightLogItem(idx);

        // Control button states
        btnPrev.disabled = (idx === 0);
        btnNext.disabled = (idx === timeline.length - 1);
    }

    function renderLogStream() {
        logStream.innerHTML = '';
        timeline.forEach((item, i) => {
            const div = document.createElement('div');
            div.className = 'log-item';
            div.id = `log-item-${i}`;
            div.textContent = `[#${i + 1}] [${item.node} - L${item.layer}] ${item.action}: ${item.details}`;
            div.addEventListener('click', () => {
                pausePlayback();
                renderStep(i);
            });
            logStream.appendChild(div);
        });
    }

    function highlightLogItem(idx) {
        document.querySelectorAll('.log-item').forEach(it => it.classList.remove('highlight'));
        const el = document.getElementById(`log-item-${idx}`);
        if (el) {
            el.classList.add('highlight');
            el.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
    }

    function stepForward() {
        if (currentStep < timeline.length - 1) {
            renderStep(currentStep + 1);
        } else {
            pausePlayback();
        }
    }

    function stepBackward() {
        if (currentStep > 0) {
            renderStep(currentStep - 1);
        }
    }

    function startPlayback() {
        if (timeline.length === 0) return;
        isPlaying = true;
        btnPlayPause.textContent = 'Pause';
        btnPlayPause.classList.remove('primary');
        playTimer = setInterval(stepForward, stepDelay);
    }

    function pausePlayback() {
        isPlaying = false;
        btnPlayPause.textContent = 'Play';
        btnPlayPause.classList.add('primary');
        if (playTimer) {
            clearInterval(playTimer);
            playTimer = null;
        }
    }

    btnPlayPause.addEventListener('click', () => {
        if (isPlaying) {
            pausePlayback();
        } else {
            if (currentStep >= timeline.length - 1) currentStep = 0;
            startPlayback();
        }
    });

    btnNext.addEventListener('click', () => {
        pausePlayback();
        stepForward();
    });

    btnPrev.addEventListener('click', () => {
        pausePlayback();
        stepBackward();
    });

    btnReset.addEventListener('click', () => {
        pausePlayback();
        renderStep(0);
    });

    // Initialize UI on load
    updateMethodOptions();
});
