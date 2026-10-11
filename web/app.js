/**
 * Conductor Studio - Studio Amp & TONE3000 Controller
 * Direct DI Bypass & Neural Amp Modeler Patch Controller
 */

// State Management
const state = {
    isDirect: false,
    activePatchId: 't3k-jcm800-80s',
    activePatchTitle: 'Marshall JCM800 2203 (1981 Cranked Lead)',
    knobs: {
        gain_db: 12.0,
        bass_db: 2.0,
        mid_db: 1.0,
        treble_db: 3.0,
        presence_db: 2.0,
        volume_db: 0.0
    },
    patches: [],
    testFiles: [],
    currentAudioFile: null,
    isPlayingTestAudio: false,
    isProcessingAudition: true,
    isLooping: true
};

let knobDebounceTimer = null;
let searchDebounceTimer = null;

// Initialize on DOM Ready
document.addEventListener('DOMContentLoaded', () => {
    initRoutingSwitcher();
    initTonestackKnobs();
    initPedalboard();
    initAddGearModal();
    initAIArchitect();
    initPatchBrowser();
    initTestAuditionScrubber();
    initHardwareModal();
    initTelemetry();
});

/* ==========================================================================
   Signal Routing: Direct DI vs TONE3000 Active
   ========================================================================== */
function initRoutingSwitcher() {
    const directBtn = document.getElementById('modeDirectBtn');
    const tone3000Btn = document.getElementById('modeTone3000Btn');

    if (directBtn) {
        directBtn.addEventListener('click', () => setDirectMode(true));
    }
    if (tone3000Btn) {
        tone3000Btn.addEventListener('click', () => setDirectMode(false));
    }
}

async function setDirectMode(enableDirect) {
    state.isDirect = enableDirect;
    updateRoutingUI(enableDirect);

    try {
        const res = await fetch('/api/set_direct_mode', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ is_direct: enableDirect })
        });
        const data = await res.json();
        console.log('[Routing Mode Updated]:', data);
    } catch (err) {
        console.error('Error toggling direct mode:', err);
    }
}

function updateRoutingUI(isDirect) {
    const directBtn = document.getElementById('modeDirectBtn');
    const tone3000Btn = document.getElementById('modeTone3000Btn');
    const masterJewel = document.getElementById('masterJewel');
    const directBanner = document.getElementById('directBanner');
    const ampPilot = document.getElementById('ampPilotLight');
    const engineTag = document.getElementById('engineStatusTag');
    const tonestackRack = document.getElementById('tonestackRack');

    if (isDirect) {
        directBtn?.classList.add('active');
        tone3000Btn?.classList.remove('active');
        masterJewel?.classList.add('direct');
        ampPilot?.classList.add('direct');
        if (directBanner) directBanner.style.display = 'flex';
        if (engineTag) {
            engineTag.textContent = 'Direct DI Bypass (0.0 ms)';
            engineTag.style.color = '#00d9f5';
        }
        if (tonestackRack) tonestackRack.style.opacity = '0.45';
    } else {
        directBtn?.classList.remove('active');
        tone3000Btn?.classList.add('active');
        masterJewel?.classList.remove('direct');
        ampPilot?.classList.remove('direct');
        if (directBanner) directBanner.style.display = 'none';
        if (engineTag) {
            engineTag.textContent = 'TONE3000 Active';
            engineTag.style.color = '#ff9e00';
        }
        if (tonestackRack) tonestackRack.style.opacity = '1.0';
    }
}

/* ==========================================================================
   Tactile Tonestack Knobs
   ========================================================================== */
function initTonestackKnobs() {
    const knobParams = ['gain', 'bass', 'mid', 'treble', 'presence', 'volume'];

    knobParams.forEach(param => {
        const input = document.getElementById(`knob_${param}`);
        const readout = document.getElementById(`val_${param}`);

        if (input) {
            input.addEventListener('input', (e) => {
                const val = parseFloat(e.target.value);
                const dbKey = `${param}_db`;
                state.knobs[dbKey] = val;

                if (readout) {
                    readout.textContent = (val > 0 ? `+${val.toFixed(1)}` : val.toFixed(1));
                }

                // If currently in Direct mode, turning an amp knob switches to TONE3000 mode automatically
                if (state.isDirect) {
                    setDirectMode(false);
                }

                // Debounce real-time knob update to server
                clearTimeout(knobDebounceTimer);
                knobDebounceTimer = setTimeout(() => {
                    sendKnobsToServer();
                }, 25);
            });
        }
    });
}

async function sendKnobsToServer() {
    try {
        await fetch('/api/set_knobs', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(state.knobs)
        });
    } catch (err) {
        console.error('Error updating knobs:', err);
    }
}

function updateKnobSliders(knobs) {
    if (!knobs) return;

    for (const [key, val] of Object.entries(knobs)) {
        const param = key.replace('_db', '');
        const input = document.getElementById(`knob_${param}`);
        const readout = document.getElementById(`val_${param}`);

        if (input && typeof val === 'number') {
            input.value = val;
            state.knobs[key] = val;
            if (readout) {
                readout.textContent = (val > 0 ? `+${val.toFixed(1)}` : val.toFixed(1));
            }
        }
    }
}

/* ==========================================================================
   AI Tone Architect & Prompt Springboard
   ========================================================================== */
function initAIArchitect() {
    const submitBtn = document.getElementById('submitBtn');
    const input = document.getElementById('commandInput');
    const chips = document.querySelectorAll('.chip');

    if (submitBtn && input) {
        submitBtn.addEventListener('click', () => {
            const prompt = input.value.trim();
            if (prompt) synthesizePrompt(prompt);
        });

        input.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                const prompt = input.value.trim();
                if (prompt) synthesizePrompt(prompt);
            }
        });
    }

    chips.forEach(chip => {
        chip.addEventListener('click', () => {
            const prompt = chip.getAttribute('data-prompt');
            if (input) input.value = prompt;
            synthesizePrompt(prompt);
        });
    });
}

async function synthesizePrompt(prompt) {
    const submitBtn = document.getElementById('submitBtn');
    const origBtnHtml = submitBtn ? submitBtn.innerHTML : '';
    if (submitBtn) {
        submitBtn.innerHTML = '<span>Synthesizing...</span>';
        submitBtn.disabled = true;
    }

    try {
        const res = await fetch('/api/process_command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: prompt })
        });
        const data = await res.json();
        console.log('[AI Response]:', data);

        // Update UI
        if (data.graph) {
            state.currentGraph = data.graph;
            renderPedalboard(data.graph);

            // Check direct DI
            if (data.graph.is_direct) {
                setDirectMode(true);
            } else {
                setDirectMode(false);
            }

            // Find amp node
            const ampNode = data.graph.nodes?.find(n => n.node_type === 'Amp');
            if (ampNode) {
                const titleEl = document.getElementById('activePatchTitle');
                const metaEl = document.getElementById('activePatchMeta');

                if (titleEl) titleEl.textContent = ampNode.patch_name || ampNode.display_name || ampNode.gear_model || 'TONE3000 Custom Amp';
                if (metaEl) metaEl.textContent = `${ampNode.gear_make || 'TONE3000'} • ${ampNode.gear_model || 'Boutique Tube Amp'} • Authentic NAM Capture`;

                // Update knob values
                updateKnobSliders(ampNode.params);
            }
        }

        // Show AI Rationale
        const rationaleCard = document.getElementById('aiRationaleCard');
        const rationaleText = document.getElementById('aiRationaleText');
        const tagsRow = document.getElementById('emotionalTagsRow');

        if (rationaleCard && (data.gear_selection_rationale || data.intent)) {
            rationaleCard.style.display = 'block';
            if (rationaleText) {
                rationaleText.textContent = data.gear_selection_rationale || data.intent;
            }
            if (tagsRow && data.emotional_tags) {
                tagsRow.innerHTML = data.emotional_tags.map(t => `<span class="tag-badge">#${t}</span>`).join('');
            }
        }

    } catch (err) {
        console.error('Error synthesizing prompt:', err);
    } finally {
        if (submitBtn) {
            submitBtn.innerHTML = origBtnHtml;
            submitBtn.disabled = false;
        }
    }
}

/* ==========================================================================
   TONE3000 Patch Browser & Library
   ========================================================================== */
async function initPatchBrowser() {
    const searchInput = document.getElementById('patchSearchInput');
    const spinner = document.getElementById('searchSpinner');

    // Fetch initial featured patches
    try {
        const res = await fetch('/api/tone3000_patches');
        const data = await res.json();
        if (data.patches) {
            state.patches = data.patches;
            renderPatches(data.patches);
        }
    } catch (err) {
        console.error('Error fetching patches:', err);
    }

    // Live search
    if (searchInput) {
        searchInput.addEventListener('input', (e) => {
            const query = e.target.value.trim();
            clearTimeout(searchDebounceTimer);
            if (spinner) spinner.style.display = 'block';

            searchDebounceTimer = setTimeout(async () => {
                try {
                    const res = await fetch(`/api/search_tones?query=${encodeURIComponent(query)}`);
                    const data = await res.json();
                    renderPatches(data.tones || data.results || []);
                } catch (err) {
                    console.error('Error searching tones:', err);
                } finally {
                    if (spinner) spinner.style.display = 'none';
                }
            }, 250);
        });
    }
}

function renderPatches(patches) {
    const grid = document.getElementById('patchesGrid');
    if (!grid) return;

    if (!patches || patches.length === 0) {
        grid.innerHTML = '<div style="color: #8392a7; font-size: 13px; grid-column: 1/-1; padding: 20px; text-align: center;">No matching TONE3000 captures found.</div>';
        return;
    }

    grid.innerHTML = patches.map(p => {
        const toneId = p.tone_id || p.id;
        const isActive = (state.activePatchId === toneId && !state.isDirect);
        const tags = (p.tags || []).slice(0, 3).map(t => `<span class="patch-tag">${t}</span>`).join('');

        return `
            <div class="patch-card ${isActive ? 'active' : ''}" data-tone-id="${toneId}">
                <div>
                    <div class="patch-card-top">
                        <span class="patch-gain-badge">${p.gain_level || 'NAM Model'}</span>
                    </div>
                    <div class="patch-name">${p.title || p.name}</div>
                    <div class="patch-model">${p.amp_model || p.author || 'TONE3000 Capture'}</div>
                </div>
                <div class="patch-tags">${tags}</div>
            </div>
        `;
    }).join('');

    // Add click handlers
    grid.querySelectorAll('.patch-card').forEach(card => {
        card.addEventListener('click', () => {
            const toneId = card.getAttribute('data-tone-id');
            selectPatch(toneId);
        });
    });
}

async function selectPatch(toneId) {
    state.activePatchId = toneId;
    state.isDirect = false;
    updateRoutingUI(false);

    // Update active card style
    document.querySelectorAll('.patch-card').forEach(card => {
        card.classList.toggle('active', card.getAttribute('data-tone-id') === toneId);
    });

    try {
        const res = await fetch('/api/select_patch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ tone_id: toneId, knobs: state.knobs })
        });
        const data = await res.json();
        console.log('[Selected Patch]:', data);

        if (data.patch) {
            const titleEl = document.getElementById('activePatchTitle');
            const metaEl = document.getElementById('activePatchMeta');

            if (titleEl) titleEl.textContent = data.patch.title || data.patch.name;
            if (metaEl) metaEl.textContent = `${data.patch.author || 'ToneMaster'} • ${data.patch.amp_model || 'Authentic Capture'} • TONE3000 NAM`;

            if (data.patch.default_knobs) {
                updateKnobSliders(data.patch.default_knobs);
            }
        }

        if (data.graph) {
            state.currentGraph = data.graph;
            renderPedalboard(data.graph);
        }
    } catch (err) {
        console.error('Error selecting patch:', err);
    }
}

/* ==========================================================================
   Dry Guitar Audition Scrubber
   ========================================================================== */
function initTestAuditionScrubber() {
    const fileSelect = document.getElementById('testFileSelect');
    const playBtn = document.getElementById('playTestBtn');
    const loopBtn = document.getElementById('loopTestBtn');
    const modeBtn = document.getElementById('processToggleBtn');
    const scrubber = document.getElementById('audioScrubber');
    const curTime = document.getElementById('testCurrentTime');
    const durTime = document.getElementById('testDuration');
    const audio = document.getElementById('testAudioElement');

    // Fetch available WAV files
    fetch('/api/test_audio_files')
        .then(r => r.json())
        .then(data => {
            if (data.files && data.files.length > 0) {
                state.testFiles = data.files;
                if (fileSelect) {
                    fileSelect.innerHTML = data.files.map(f => `<option value="${f.filename}">${f.name}</option>`).join('');
                    loadTestAudio(data.files[0].filename);
                }
            }
        });

    if (fileSelect) {
        fileSelect.addEventListener('change', (e) => {
            loadTestAudio(e.target.value);
        });
    }

    if (playBtn && audio) {
        playBtn.addEventListener('click', () => {
            if (audio.paused) {
                audio.play();
                playBtn.textContent = '⏸ Pause';
            } else {
                audio.pause();
                playBtn.textContent = '▶ Play Track';
            }
        });
    }

    if (loopBtn && audio) {
        loopBtn.addEventListener('click', () => {
            state.isLooping = !state.isLooping;
            audio.loop = state.isLooping;
            loopBtn.classList.toggle('active', state.isLooping);
        });
    }

    if (modeBtn) {
        modeBtn.addEventListener('click', () => {
            state.isProcessingAudition = !state.isProcessingAudition;
            modeBtn.textContent = state.isProcessingAudition ? '⚡ DSP Processed' : '🔌 Raw Dry DI';
            modeBtn.style.color = state.isProcessingAudition ? '#ff9e00' : '#00d9f5';
            modeBtn.style.borderColor = state.isProcessingAudition ? '#ff9e00' : '#00d9f5';
            const curFile = fileSelect?.value;
            if (curFile) loadTestAudio(curFile);
        });
    }

    if (audio) {
        audio.addEventListener('timeupdate', () => {
            if (audio.duration && scrubber) {
                const pct = (audio.currentTime / audio.duration) * 100;
                scrubber.value = pct;
                if (curTime) curTime.textContent = formatTime(audio.currentTime);
                if (durTime) durTime.textContent = formatTime(audio.duration);
            }
        });

        audio.addEventListener('ended', () => {
            if (!state.isLooping && playBtn) {
                playBtn.textContent = '▶ Play Track';
            }
        });
    }

    if (scrubber && audio) {
        scrubber.addEventListener('input', (e) => {
            if (audio.duration) {
                audio.currentTime = (e.target.value / 100) * audio.duration;
            }
        });
    }
}

function loadTestAudio(filename) {
    const audio = document.getElementById('testAudioElement');
    const playBtn = document.getElementById('playTestBtn');
    if (!audio) return;

    const wasPlaying = !audio.paused;

    if (state.isProcessingAudition) {
        fetch('/api/process_test_audio', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ filename })
        })
        .then(r => r.blob())
        .then(blob => {
            audio.src = URL.createObjectURL(blob);
            audio.loop = state.isLooping;
            if (wasPlaying) {
                audio.play();
                if (playBtn) playBtn.textContent = '⏸ Pause';
            }
        })
        .catch(err => console.error('Error processing audition file:', err));
    } else {
        audio.src = `/api/test_audio_stream/${encodeURIComponent(filename)}`;
        audio.loop = state.isLooping;
        if (wasPlaying) {
            audio.play();
            if (playBtn) playBtn.textContent = '⏸ Pause';
        }
    }
}

function formatTime(secs) {
    if (!secs || isNaN(secs)) return '00:00';
    const m = Math.floor(secs / 60);
    const s = Math.floor(secs % 60);
    return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
}

/* ==========================================================================
   Hardware Audio Interface Routing Modal
   ========================================================================== */
function initHardwareModal() {
    const openBtn = document.getElementById('openHardwareBtn');
    const closeBtn = document.getElementById('closeHardwareModal');
    const cancelBtn = document.getElementById('cancelHardwareBtn');
    const saveBtn = document.getElementById('saveHardwareBtn');
    const modal = document.getElementById('hardwareModal');

    const inDevSelect = document.getElementById('inputDevSelect');
    const outDevSelect = document.getElementById('outputDevSelect');
    const guitarChSelect = document.getElementById('guitarChSelect');
    const micChSelect = document.getElementById('micChSelect');

    const openModal = async () => {
        if (!modal) return;
        modal.style.display = 'flex';

        try {
            const res = await fetch('/api/audio_devices');
            const data = await res.json();

            if (inDevSelect && outDevSelect && data.devices) {
                inDevSelect.innerHTML = data.devices.map(d => `<option value="${d.id}">${d.name} (${d.max_input_channels} in)</option>`).join('');
                outDevSelect.innerHTML = data.devices.map(d => `<option value="${d.id}">${d.name} (${d.max_output_channels} out)</option>`).join('');

                if (data.current_config) {
                    if (data.current_config.input_device_id !== null) inDevSelect.value = data.current_config.input_device_id;
                    if (data.current_config.output_device_id !== null) outDevSelect.value = data.current_config.output_device_id;
                    if (guitarChSelect) guitarChSelect.value = data.current_config.guitar_channel_idx;
                    if (micChSelect) micChSelect.value = data.current_config.mic_channel_idx;
                }
            }
        } catch (err) {
            console.error('Error fetching audio devices:', err);
        }
    };

    const closeModal = () => {
        if (modal) modal.style.display = 'none';
    };

    if (openBtn) openBtn.addEventListener('click', openModal);
    if (closeBtn) closeBtn.addEventListener('click', closeModal);
    if (cancelBtn) cancelBtn.addEventListener('click', closeModal);

    if (saveBtn) {
        saveBtn.addEventListener('click', async () => {
            saveBtn.textContent = 'Applying...';
            try {
                await fetch('/api/set_audio_device', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        input_device_id: inDevSelect?.value,
                        output_device_id: outDevSelect?.value,
                        guitar_channel_idx: parseInt(guitarChSelect?.value || '1'),
                        mic_channel_idx: parseInt(micChSelect?.value || '0')
                    })
                });
                closeModal();
            } catch (err) {
                console.error('Error saving audio hardware settings:', err);
            } finally {
                saveBtn.textContent = 'Apply Hardware Routing';
            }
        });
    }
}

/* ==========================================================================
   Pedalboard & Signal Chain Rack
   ========================================================================== */
function initPedalboard() {
    // Fetch initial graph
    fetch('/api/current_graph')
        .then(r => r.json())
        .then(data => {
            if (data.graph) {
                state.currentGraph = data.graph;
                renderPedalboard(data.graph);
            }
        })
        .catch(err => console.error('Error fetching current graph:', err));
}

function renderPedalboard(graph) {
    const rack = document.getElementById('pedalboardRack');
    if (!rack) return;

    if (!graph || !graph.nodes || graph.nodes.length === 0) {
        rack.innerHTML = `
            <div style="color: #63738a; font-size: 13px; padding: 24px; text-align: center; width: 100%;">
                Direct DI Mode active (No pedals in signal chain). Click "+ Add Stompbox / Space" or select an authentic TONE3000 capture.
            </div>
        `;
        return;
    }

    rack.innerHTML = '';

    graph.nodes.forEach((node, idx) => {
        const isAmp = (node.node_type === 'Amp');
        const isBypassed = !node.enabled;
        const gearType = (node.gear_type || (isAmp ? 'Amp' : (node.node_type === 'Reverb' ? 'Space' : 'Stompbox'))).toUpperCase();

        // Theme determination
        let themeClass = 'theme-amp';
        let ledColorClass = 'on';

        if (!isAmp) {
            const lowName = (node.display_name || node.gear_model || node.node_type || '').toLowerCase();
            if (node.node_type === 'Distortion') {
                themeClass = 'theme-distortion';
                ledColorClass = 'on green';
            } else if (node.node_type === 'Chorus' || node.node_type === 'Phaser') {
                themeClass = 'theme-chorus';
                ledColorClass = 'on blue';
            } else if (node.node_type === 'Delay') {
                themeClass = 'theme-delay';
                ledColorClass = 'on';
            } else if (node.node_type === 'Reverb' || gearType === 'SPACE') {
                themeClass = 'theme-space';
                ledColorClass = 'on blue';
            } else if (node.node_type === 'Compressor') {
                themeClass = 'theme-compressor';
                ledColorClass = 'on';
            } else if (node.node_type === 'NoiseGate') {
                themeClass = 'theme-noisegate';
                ledColorClass = 'on green';
            }
        }

        const card = document.createElement('div');
        card.className = `pedal-card ${themeClass} ${isBypassed ? 'bypassed' : ''}`;
        card.dataset.index = idx;

        // Knobs generation
        let knobsHtml = '';
        if (node.params && Object.keys(node.params).length > 0) {
            knobsHtml = '<div class="pedal-knobs-grid">';
            for (const [pKey, pVal] of Object.entries(node.params)) {
                // Determine sensible slider range
                let min = 0, max = 1, step = 0.05;
                if (pKey.includes('_db')) {
                    min = -24; max = 36; step = 0.5;
                } else if (pKey.includes('hz')) {
                    min = 0.1; max = 10.0; step = 0.1;
                } else if (pKey.includes('seconds')) {
                    min = 0.05; max = 1.2; step = 0.01;
                } else if (pKey.includes('ms')) {
                    min = 1.0; max = 100.0; step = 1.0;
                } else if (pKey.includes('ratio')) {
                    min = 1.0; max = 20.0; step = 0.5;
                }

                const shortLabel = pKey.replace('_db', '').replace('_seconds', '').replace('_hz', '').replace('_ms', '');
                const valDisplay = (typeof pVal === 'number' ? (pVal % 1 === 0 ? pVal : pVal.toFixed(1)) : pVal);

                knobsHtml += `
                    <div class="pedal-knob-unit" data-param="${pKey}">
                        <span class="pedal-knob-label">${shortLabel}</span>
                        <input type="range" class="pedal-slider" data-param="${pKey}" min="${min}" max="${max}" step="${step}" value="${pVal}">
                        <span class="pedal-knob-val">${valDisplay}</span>
                    </div>
                `;
            }
            knobsHtml += '</div>';
        }

        card.innerHTML = `
            <div>
                <div class="pedal-top-row">
                    <span class="pedal-type-tag">${gearType}</span>
                    ${!isAmp ? `<button class="btn-remove-gear" title="Remove from chain" data-index="${idx}">&times;</button>` : ''}
                </div>
                <div class="pedal-name">${node.display_name || node.gear_model || node.node_type}</div>
                <div class="pedal-model">${node.gear_make || node.gear_model || 'TONE3000 Capture'}</div>
            </div>

            ${knobsHtml}

            <div class="pedal-footswitch-box">
                <div class="footswitch-led ${node.enabled ? ledColorClass : ''}"></div>
                <button class="footswitch-btn" title="${node.enabled ? 'Click to Bypass' : 'Click to Engage'}" data-index="${idx}"></button>
            </div>
        `;

        // Wire slider inputs
        card.querySelectorAll('.pedal-slider').forEach(slider => {
            slider.addEventListener('input', (e) => {
                const paramName = e.target.getAttribute('data-param');
                const val = parseFloat(e.target.value);
                const readout = e.target.parentElement.querySelector('.pedal-knob-val');
                if (readout) readout.textContent = (val % 1 === 0 ? val : val.toFixed(1));

                node.params[paramName] = val;

                // If amp tonestack knob, sync with main chassis slider
                if (isAmp) {
                    const mainInput = document.getElementById(`knob_${paramName.replace('_db', '')}`);
                    const mainReadout = document.getElementById(`val_${paramName.replace('_db', '')}`);
                    if (mainInput) mainInput.value = val;
                    if (mainReadout) mainReadout.textContent = (val > 0 ? `+${val.toFixed(1)}` : val.toFixed(1));
                }

                clearTimeout(knobDebounceTimer);
                knobDebounceTimer = setTimeout(async () => {
                    try {
                        await fetch('/api/set_gear_param', {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({ index: idx, param: paramName, value: val })
                        });
                    } catch (err) {
                        console.error('Error updating gear param:', err);
                    }
                }, 40);
            });
        });

        // Wire footswitch click
        const footswitch = card.querySelector('.footswitch-btn');
        if (footswitch) {
            footswitch.addEventListener('click', async () => {
                try {
                    const res = await fetch('/api/toggle_gear', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ index: idx })
                    });
                    const data = await res.json();
                    if (data.graph) {
                        state.currentGraph = data.graph;
                        renderPedalboard(data.graph);
                    }
                } catch (err) {
                    console.error('Error toggling gear:', err);
                }
            });
        }

        // Wire remove click
        const removeBtn = card.querySelector('.btn-remove-gear');
        if (removeBtn) {
            removeBtn.addEventListener('click', async (e) => {
                e.stopPropagation();
                try {
                    const res = await fetch('/api/remove_gear', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ index: idx })
                    });
                    const data = await res.json();
                    if (data.graph) {
                        state.currentGraph = data.graph;
                        renderPedalboard(data.graph);
                    }
                } catch (err) {
                    console.error('Error removing gear:', err);
                }
            });
        }

        rack.appendChild(card);

        // Add connector arrow if not last node
        if (idx < graph.nodes.length - 1) {
            const arrow = document.createElement('div');
            arrow.className = 'pedal-connector-arrow';
            arrow.innerHTML = '➔';
            rack.appendChild(arrow);
        }
    });
}

/* ==========================================================================
   Add Gear Modal (TONE3000 Stompboxes & Spaces)
   ========================================================================== */
function initAddGearModal() {
    const modal = document.getElementById('addGearModal');
    const openBtn = document.getElementById('openAddGearBtn');
    const closeBtn = document.getElementById('closeAddGearModal');
    const tabStompboxes = document.getElementById('tabStompboxes');
    const tabSpaces = document.getElementById('tabSpaces');
    const grid = document.getElementById('gearPickerGrid');

    let cachedCatalog = null;
    let activeTab = 'stompbox';

    const renderCatalogGrid = () => {
        if (!grid || !cachedCatalog) return;
        const items = (activeTab === 'stompbox' ? cachedCatalog.stompboxes : cachedCatalog.spaces) || [];

        grid.innerHTML = items.map(item => `
            <div class="gear-picker-item">
                <div>
                    <span class="patch-gain-badge" style="margin-bottom: 6px;">${item.gear_type}</span>
                    <div class="gear-picker-name">${item.title}</div>
                    <div class="gear-picker-model">${item.author || item.amp_model}</div>
                    <div class="patch-tags" style="margin-bottom: 10px;">
                        ${(item.tags || []).slice(0, 3).map(t => `<span class="patch-tag">${t}</span>`).join('')}
                    </div>
                </div>
                <button class="btn-insert-gear" data-tone-id="${item.tone_id}">+ Add to Board</button>
            </div>
        `).join('');

        grid.querySelectorAll('.btn-insert-gear').forEach(btn => {
            btn.addEventListener('click', async () => {
                const toneId = btn.getAttribute('data-tone-id');
                btn.textContent = 'Adding...';
                try {
                    const res = await fetch('/api/add_gear', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ tone_id: toneId })
                    });
                    const data = await res.json();
                    if (data.graph) {
                        state.currentGraph = data.graph;
                        renderPedalboard(data.graph);
                    }
                    if (modal) modal.style.display = 'none';
                } catch (err) {
                    console.error('Error adding gear:', err);
                } finally {
                    btn.textContent = '+ Add to Board';
                }
            });
        });
    };

    const openModal = async () => {
        if (modal) modal.style.display = 'flex';
        if (!cachedCatalog) {
            try {
                const res = await fetch('/api/gear_catalog');
                cachedCatalog = await res.json();
            } catch (err) {
                console.error('Error fetching gear catalog:', err);
            }
        }
        renderCatalogGrid();
    };

    if (openBtn) openBtn.addEventListener('click', openModal);
    if (closeBtn) closeBtn.addEventListener('click', () => { if (modal) modal.style.display = 'none'; });

    if (tabStompboxes) {
        tabStompboxes.addEventListener('click', () => {
            activeTab = 'stompbox';
            tabStompboxes.classList.add('active');
            tabSpaces?.classList.remove('active');
            renderCatalogGrid();
        });
    }

    if (tabSpaces) {
        tabSpaces.addEventListener('click', () => {
            activeTab = 'space';
            tabSpaces.classList.add('active');
            tabStompboxes?.classList.remove('active');
            renderCatalogGrid();
        });
    }
}

/* ==========================================================================
   Telemetry: Live Guitar Tuner, dBFS Meters, and Clipping Detection
   ========================================================================== */
let browserMicStream = null;
let browserAudioCtx = null;
let browserAnalyser = null;
let browserPitchTimer = null;

function initTelemetry() {
    const inputVu = document.getElementById('inputVuBar');
    const inputPeak = document.getElementById('inputPeakMarker');
    const inputDb = document.getElementById('inputDbReadout');
    const inputClip = document.getElementById('inputClipBadge');

    const outputVu = document.getElementById('outputVuBar');
    const outputPeak = document.getElementById('outputPeakMarker');
    const outputDb = document.getElementById('outputDbReadout');
    const outputClip = document.getElementById('outputClipBadge');

    const tunerNote = document.getElementById('tunerNote');
    const tunerOctave = document.getElementById('tunerOctave');
    const tunerCents = document.getElementById('tunerCentsVal');
    const tunerFreq = document.getElementById('tunerFreqReadout');
    const tunerNeedle = document.getElementById('tunerNeedle');
    const tunerLock = document.getElementById('tunerLockBadge');
    const tunerJewel = document.getElementById('tunerJewel');
    const micBtn = document.getElementById('browserMicBtn');

    // Helper: Map dBFS (-60 to 0) to percentage (0% to 100%)
    const dbToPercent = (db) => {
        if (db <= -60) return 0;
        if (db >= 0) return 100;
        return Math.min(Math.max(((db + 60) / 60) * 100, 0), 100);
    };

    // 1. Polling hardware stream telemetry every 70ms
    setInterval(async () => {
        if (browserMicStream) return; // If browser mic active, let browser analyser drive

        try {
            const res = await fetch('/api/telemetry');
            const data = await res.json();

            // Update Input Meter
            const inPct = dbToPercent(data.input_peak_db || -60);
            if (inputVu) inputVu.style.width = `${inPct}%`;
            if (inputPeak) inputPeak.style.left = `${inPct}%`;
            if (inputDb) inputDb.textContent = (data.input_peak_db !== undefined ? data.input_peak_db.toFixed(1) : '-60.0');
            if (inputClip) inputClip.classList.toggle('active', !!data.input_clipping);

            // Update Output Meter
            const outPct = dbToPercent(data.output_peak_db || -60);
            if (outputVu) outputVu.style.width = `${outPct}%`;
            if (outputPeak) outputPeak.style.left = `${outPct}%`;
            if (outputDb) outputDb.textContent = (data.output_peak_db !== undefined ? data.output_peak_db.toFixed(1) : '-60.0');
            if (outputClip) outputClip.classList.toggle('active', !!data.output_clipping);

            // Update Tuner
            if (data.tuner && data.tuner.detected) {
                if (tunerNote) {
                    tunerNote.textContent = data.tuner.note;
                    tunerNote.classList.toggle('in-tune', !!data.tuner.in_tune);
                }
                if (tunerOctave) tunerOctave.textContent = data.tuner.octave;
                if (tunerFreq) tunerFreq.textContent = `${data.tuner.freq_hz} Hz`;
                if (tunerCents) tunerCents.textContent = (data.tuner.cents > 0 ? `+${data.tuner.cents}` : `${data.tuner.cents}`);

                if (tunerNeedle) {
                    // Cents scale: 50% center, range [-50, +50] maps to [5%, 95%]
                    const needlePct = Math.min(Math.max(50 + (data.tuner.cents / 50) * 45, 5), 95);
                    tunerNeedle.style.left = `${needlePct}%`;
                    tunerNeedle.className = `tuner-needle ${data.tuner.in_tune ? 'in-tune' : (data.tuner.cents < 0 ? 'flat' : 'sharp')}`;
                }

                if (tunerLock) tunerLock.classList.toggle('active', !!data.tuner.in_tune);
                if (tunerJewel) tunerJewel.style.background = data.tuner.in_tune ? '#10b981' : '#ff9e00';
            } else {
                if (tunerNote) {
                    tunerNote.textContent = '--';
                    tunerNote.classList.remove('in-tune');
                }
                if (tunerOctave) tunerOctave.textContent = '';
                if (tunerFreq) tunerFreq.textContent = '-- Hz';
                if (tunerCents) tunerCents.textContent = '0.0';
                if (tunerNeedle) {
                    tunerNeedle.style.left = '50%';
                    tunerNeedle.className = 'tuner-needle';
                }
                if (tunerLock) tunerLock.classList.remove('active');
                if (tunerJewel) tunerJewel.style.background = '#4b586e';
            }

        } catch (err) {
            // Server offline or stream inactive
        }
    }, 70);

    // 2. Browser Mic Fallback / Live Tester
    if (micBtn) {
        micBtn.addEventListener('click', async () => {
            if (browserMicStream) {
                // Stop mic
                browserMicStream.getTracks().forEach(t => t.stop());
                browserMicStream = null;
                if (browserAudioCtx) browserAudioCtx.close();
                browserAudioCtx = null;
                micBtn.classList.remove('active');
                micBtn.innerHTML = '<span>🎙 Browser Mic</span>';
                return;
            }

            try {
                const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
                browserMicStream = stream;
                browserAudioCtx = new (window.AudioContext || window.webkitAudioContext)();
                const source = browserAudioCtx.createMediaStreamSource(stream);
                browserAnalyser = browserAudioCtx.createAnalyser();
                browserAnalyser.fftSize = 2048;
                source.connect(browserAnalyser);

                micBtn.classList.add('active');
                micBtn.innerHTML = '<span>🔴 Mic Listening</span>';

                const buf = new Float32Array(browserAnalyser.fftSize);

                const processMicFrame = () => {
                    if (!browserMicStream) return;
                    browserAnalyser.getFloatTimeDomainData(buf);

                    let peak = 0;
                    let sumSq = 0;
                    for (let i = 0; i < buf.length; i++) {
                        const abs = Math.abs(buf[i]);
                        if (abs > peak) peak = abs;
                        sumSq += abs * abs;
                    }
                    const rms = Math.sqrt(sumSq / buf.length);
                    const peakDb = 20 * Math.log10(Math.max(peak, 1e-4));

                    const inPct = dbToPercent(peakDb);
                    if (inputVu) inputVu.style.width = `${inPct}%`;
                    if (inputDb) inputDb.textContent = peakDb.toFixed(1);
                    if (inputClip) inputClip.classList.toggle('active', peak >= 0.98);

                    // Autocorrelation pitch detection
                    if (rms > 0.008) {
                        const sr = browserAudioCtx.sampleRate;
                        const minLag = Math.floor(sr / 1000);
                        const maxLag = Math.floor(sr / 65);

                        let bestLag = -1;
                        let bestCorr = 0;

                        for (let lag = minLag; lag < maxLag; lag++) {
                            let corr = 0;
                            for (let i = 0; i < buf.length - lag; i++) {
                                corr += buf[i] * buf[i + lag];
                            }
                            if (corr > bestCorr) {
                                bestCorr = corr;
                                bestLag = lag;
                            }
                        }

                        if (bestLag > 0 && bestCorr > 0.4) {
                            const freq = sr / bestLag;
                            if (freq >= 60 && freq <= 1100) {
                                const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
                                const midiNum = 12 * Math.log2(freq / 440) + 69;
                                const closest = Math.round(midiNum);
                                const note = noteNames[closest % 12];
                                const octave = Math.floor(closest / 12) - 1;
                                const cents = (midiNum - closest) * 100;
                                const inTune = Math.abs(cents) <= 3.0;

                                if (tunerNote) {
                                    tunerNote.textContent = note;
                                    tunerNote.classList.toggle('in-tune', inTune);
                                }
                                if (tunerOctave) tunerOctave.textContent = octave;
                                if (tunerFreq) tunerFreq.textContent = `${freq.toFixed(1)} Hz`;
                                if (tunerCents) tunerCents.textContent = (cents > 0 ? `+${cents.toFixed(1)}` : cents.toFixed(1));

                                if (tunerNeedle) {
                                    const needlePct = Math.min(Math.max(50 + (cents / 50) * 45, 5), 95);
                                    tunerNeedle.style.left = `${needlePct}%`;
                                    tunerNeedle.className = `tuner-needle ${inTune ? 'in-tune' : (cents < 0 ? 'flat' : 'sharp')}`;
                                }
                                if (tunerLock) tunerLock.classList.toggle('active', inTune);
                                if (tunerJewel) tunerJewel.style.background = inTune ? '#10b981' : '#ff9e00';
                            }
                        }
                    }

                    requestAnimationFrame(processMicFrame);
                };

                requestAnimationFrame(processMicFrame);

            } catch (err) {
                console.error('Error enabling browser microphone:', err);
                alert('Could not access microphone: ' + err.message);
            }
        });
    }
}
