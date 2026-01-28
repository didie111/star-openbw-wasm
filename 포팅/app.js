(() => {
  const $ = (id) => document.getElementById(id);

  const statusEl = $('status');
  const mpqPatch = $('mpqPatch');
  const mpqBroo = $('mpqBroo');
  const mpqStar = $('mpqStar');
  const mapFile = $('mapFile');
  const btnMount = $('btnMount');
  const btnStart = $('btnStart');

  const logLines = [];
  function setStatus(t) {
    statusEl.textContent = t;
  }

  window.__openbw_log = (t) => {
    const s = String(t);
    logLines.push(s);
    if (logLines.length > 200) logLines.shift();
    setStatus(logLines.join('\n'));
    console.log(s);
  };

  function hasAllFiles() {
    return !!(mpqPatch.files?.[0] && mpqBroo.files?.[0] && mpqStar.files?.[0]);
  }

  function updateButtons() {
    btnMount.disabled = !hasAllFiles() || !window.Module || !window.Module.calledRun;
    btnStart.disabled = !window.__openbw_mounted || !window.Module || !window.Module.calledRun;
  }

  async function writeFileToFS(file, targetPath) {
    const buf = await file.arrayBuffer();
    const u8 = new Uint8Array(buf);
    try { Module.FS.mkdir('/data'); } catch (_) {}
    Module.FS.writeFile(targetPath, u8);
    window.__openbw_log(`[FS] write ${targetPath} (${u8.byteLength} bytes)`);
    try {
      const ap = Module.FS.analyzePath(targetPath);
      window.__openbw_log(`[FS] exists ${targetPath}: ${!!ap?.exists}`);
    } catch (e) {
      window.__openbw_log(`[FS] analyzePath failed: ${e?.stack || e}`);
    }
  }

  async function mountMpqs() {
    window.__openbw_log('[MPQ] mounting...');
    await writeFileToFS(mpqPatch.files[0], '/data/Patch_rt.mpq');
    await writeFileToFS(mpqBroo.files[0], '/data/BrooDat.mpq');
    await writeFileToFS(mpqStar.files[0], '/data/StarDat.mpq');

    try { Module.FS.mkdir('/data/maps'); } catch (_) {}
    if (mapFile?.files?.[0]) {
      await writeFileToFS(mapFile.files[0], '/data/maps/(2)Astral Balance.scm');
    }

    // Optional: persist in IndexedDB later (IDBFS) if needed.
    window.__openbw_mounted = true;
    window.__openbw_log('[MPQ] mounted. Ready to start.');
    updateButtons();
  }

  function startGame() {
    if (!window.__openbw_mounted) {
      window.__openbw_log('[ERR] MPQ not mounted');
      return;
    }
    // Call into WASM to start.
    // We expose a C function `openbw_web_start(const char* dataDir)`.
    const fn = Module.cwrap('openbw_web_start', 'number', ['string']);
    const rc = fn('/data');
    window.__openbw_log(`[WASM] openbw_web_start rc=${rc}`);
  }

  btnMount.addEventListener('click', () => {
    mountMpqs().catch((e) => {
      window.__openbw_log(`[ERR] mount failed: ${e?.stack || e}`);
    });
  });

  btnStart.addEventListener('click', () => {
    try {
      startGame();
    } catch (e) {
      window.__openbw_log(`[ERR] start failed: ${e?.stack || e}`);
    }
  });

  [mpqPatch, mpqBroo, mpqStar].forEach((el) => {
    el.addEventListener('change', () => {
      window.__openbw_mounted = false;
      updateButtons();
    });
  });

  if (mapFile) {
    mapFile.addEventListener('change', () => {
      window.__openbw_mounted = false;
      updateButtons();
    });
  }

  const canvas = $('canvas');
  if (canvas) {
    canvas.addEventListener('contextmenu', (e) => e.preventDefault());
    canvas.addEventListener('mousedown', (e) => {
      if (e.button === 2) e.preventDefault();
    });
    canvas.addEventListener('mouseup', (e) => {
      if (e.button === 2) e.preventDefault();
    });
  }

  // Hook into Emscripten lifecycle
  const prevStatus = Module.setStatus;
  Module.setStatus = (text) => {
    if (prevStatus) prevStatus(text);
    if (text) window.__openbw_log(`[EMCC] ${text}`);
  };

  Module.onRuntimeInitialized = () => {
    window.__openbw_log('[WASM] runtime initialized');
    updateButtons();
  };

  // calledRun is set after runtime is ready; poll once
  const t = setInterval(() => {
    if (window.Module && window.Module.calledRun) {
      clearInterval(t);
      window.__openbw_log('[WASM] module calledRun');
      updateButtons();
    }
  }, 200);

  setStatus('WASM 로딩 대기중...');
})();
