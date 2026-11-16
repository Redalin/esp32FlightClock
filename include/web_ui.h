#pragma once

#include <pgmspace.h>

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Flight Clock</title>
  <style>
    :root {
      color-scheme: dark light;
      --bg: #0f172a;
      --card: #1e293b;
      --border: #334155;
      --text: #f1f5f9;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --accent-hover: #0ea5e9;
      --danger: #f87171;
      --danger-bg: rgba(248, 113, 113, 0.14);
      --ok: #4ade80;
    }
    @media (prefers-color-scheme: light) {
      :root {
        --bg: #f8fafc;
        --card: #ffffff;
        --border: #94a3b8;
        --text: #0f172a;
        --muted: #475569;
        --accent: #0284c7;
        --accent-hover: #0369a1;
        --danger: #dc2626;
        --danger-bg: #fef2f2;
        --ok: #16a34a;
      }
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      padding: 1rem;
      font-family: system-ui, -apple-system, sans-serif;
      line-height: 1.5;
      background: var(--bg);
      color: var(--text);
    }
    main {
      max-width: 36rem;
      margin: 0 auto;
      display: flex;
      flex-direction: column;
      gap: 1.25rem;
    }
    h1 {
      margin: 0;
      font-size: 1.5rem;
      line-height: 1.2;
    }
    .subtitle {
      margin: 0.25rem 0 0;
      color: var(--muted);
      font-size: 0.95rem;
    }
    .card, fieldset {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 0.75rem;
      padding: 1.125rem;
      margin: 0;
    }
    legend {
      font-weight: 700;
      padding: 0 0.5rem;
      color: var(--accent);
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(11rem, 1fr));
      gap: 0.75rem;
    }
    .stat {
      padding: 0.625rem 0.75rem;
      border-radius: 0.5rem;
      border: 1px solid var(--border);
    }
    .stat-label {
      font-size: 0.8rem;
      color: var(--muted);
      display: block;
    }
    .stat-val {
      font-weight: 600;
      font-size: 1rem;
      word-break: break-word;
    }
    .field {
      display: flex;
      flex-direction: column;
      align-items: stretch;
      gap: 0.25rem;
      margin-block-end: 1.125rem;
    }
    .field:last-child { margin-block-end: 0; }
    label {
      font-weight: 600;
      font-size: 0.95rem;
    }
    .hint {
      font-size: 0.85rem;
      color: var(--muted);
    }
    input, select, button {
      font: inherit;
      font-size: max(1rem, 1em);
      border-radius: 0.5rem;
      min-height: 48px;
    }
    input[type="text"],
    input[type="password"],
    input[type="number"],
    select {
      width: 100%;
      padding: 0.625rem 0.75rem;
      background: var(--bg);
      color: var(--text);
      border: 1.5px solid var(--border);
    }
    input:focus-visible, select:focus-visible, button:focus-visible {
      outline: 3px solid var(--accent);
      outline-offset: 2px;
    }
    :is(input:user-invalid, select:user-invalid, .user-invalid-fallback) {
      border-color: var(--danger);
      background-color: var(--danger-bg);
    }
    .error-msg {
      display: none;
      color: var(--danger);
      font-size: 0.875rem;
      margin-top: 0.2rem;
    }
    :is(input:user-invalid, select:user-invalid, .user-invalid-fallback) + .error-msg,
    .input-row:has(:is(input:user-invalid, .user-invalid-fallback)) + .error-msg {
      display: block;
    }
    .input-row {
      display: flex;
      gap: 0.5rem;
      align-items: stretch;
    }
    .input-row input { flex: 1; min-width: 0; }
    .check-row {
      display: flex;
      align-items: center;
      gap: 0.625rem;
      min-height: 48px;
    }
    .check-row input[type="checkbox"] {
      width: 1.35rem;
      height: 1.35rem;
      min-height: 0;
      accent-color: var(--accent);
    }
    .check-row label {
      font-weight: 500;
      padding: 0.5rem 0;
    }
    button {
      cursor: pointer;
      padding: 0.625rem 1rem;
      border: 1px solid var(--border);
      background: var(--bg);
      color: var(--text);
      font-weight: 600;
    }
    button.primary {
      background: var(--accent);
      color: #082f49;
      border-color: transparent;
    }
    button.primary:hover { background: var(--accent-hover); color: #ffffff; }
    .actions {
      display: flex;
      flex-wrap: wrap;
      gap: 0.75rem;
    }
    .actions button { flex: 1 1 10rem; }
    .banner {
      padding: 0.75rem 1rem;
      border-radius: 0.5rem;
      border: 1px solid var(--border);
      display: none;
    }
    .banner.show { display: block; }
    .banner.ok { border-color: var(--ok); color: var(--ok); }
    .banner.err { border-color: var(--danger); color: var(--danger); }
    .visually-hidden {
      position: absolute;
      clip-path: inset(50%);
      overflow: hidden;
      width: 1px;
      height: 1px;
      margin: -1px;
      padding: 0;
      border: 0;
      white-space: nowrap;
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>ESP32 Flight Clock</h1>
      <p class="subtitle">1.28&Prime; Round TFT &bull; Time, Weather &amp; Overhead Flight Radar</p>
    </header>

    <section class="card" aria-labelledby="status-heading">
      <h2 id="status-heading" style="margin:0 0 0.75rem;font-size:1.1rem;">Live Status</h2>
      <div class="status-grid">
        <div class="stat"><span class="stat-label">Wi-Fi / IP</span><span class="stat-val" id="st-wifi">Loading...</span></div>
        <div class="stat"><span class="stat-label">Local Time</span><span class="stat-val" id="st-time">--:--</span></div>
        <div class="stat"><span class="stat-label">Weather</span><span class="stat-val" id="st-weather">--</span></div>
        <div class="stat"><span class="stat-label">Overhead Flight</span><span class="stat-val" id="st-flight">None in range</span></div>
      </div>
      <div class="actions" style="margin-top:0.875rem;">
        <button type="button" id="btn-test-flight">Test Flight Screen</button>
        <button type="button" id="btn-refresh">Refresh Status</button>
      </div>
    </section>

    <div id="save-banner" class="banner" role="status" aria-live="polite"></div>

    <form id="config-form" method="POST" action="/api/config" aria-labelledby="form-heading" novalidate>
      <h2 id="form-heading" class="visually-hidden">Device Configuration</h2>

      <fieldset style="margin-bottom:1.25rem;">
        <legend>Wi-Fi Connectivity</legend>

        <div class="field">
          <label for="wifiSsid">Wi-Fi Network (SSID) <span aria-hidden="true">*</span></label>
          <span id="ssid-hint" class="hint">2.4 GHz Wi-Fi network name. Use Scan to pick a nearby network.</span>
          <div class="input-row">
            <input type="text" id="wifiSsid" name="wifiSsid" list="ssid-list" required maxlength="32"
              autocorrect="off" autocapitalize="none" spellcheck="false" enterkeyhint="next"
              aria-describedby="ssid-hint" data-error="ssid-error">
            <button type="button" id="btn-scan">Scan</button>
          </div>
          <div id="ssid-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter a Wi-Fi network name (1&ndash;32 characters).
          </div>
          <datalist id="ssid-list"></datalist>
        </div>

        <div class="field">
          <label for="wifiPass">Wi-Fi Password</label>
          <span id="pass-hint" class="hint">Leave blank for open networks, or keep existing if unchanged.</span>
          <div class="input-row">
            <input type="password" id="wifiPass" name="wifiPass" maxlength="64"
              autocomplete="current-password" enterkeyhint="next" aria-describedby="pass-hint">
            <button type="button" id="btn-toggle-pass" aria-pressed="false" aria-label="Show password">Show</button>
          </div>
        </div>
      </fieldset>

      <fieldset style="margin-bottom:1.25rem;">
        <legend>GPS Location</legend>

        <div class="field">
          <label for="latitude">Latitude <span aria-hidden="true">*</span></label>
          <span id="lat-hint" class="hint">Decimal degrees between -90 and 90 (e.g. -36.8485).</span>
          <input type="text" id="latitude" name="latitude" inputmode="decimal" required
            pattern="-?(90(\.0+)?|([1-8]?\d)(\.\d+)?)" enterkeyhint="next"
            aria-describedby="lat-hint" data-error="lat-error">
          <div id="lat-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter a valid latitude between -90 and 90.
          </div>
        </div>

        <div class="field">
          <label for="longitude">Longitude <span aria-hidden="true">*</span></label>
          <span id="lon-hint" class="hint">Decimal degrees between -180 and 180 (e.g. 174.7633).</span>
          <input type="text" id="longitude" name="longitude" inputmode="decimal" required
            pattern="-?(180(\.0+)?|(1[0-7]\d|[1-9]?\d)(\.\d+)?)" enterkeyhint="next"
            aria-describedby="lon-hint" data-error="lon-error">
          <div id="lon-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter a valid longitude between -180 and 180.
          </div>
        </div>

        <button type="button" id="btn-geo">Use Browser GPS</button>
      </fieldset>

      <fieldset style="margin-bottom:1.25rem;">
        <legend>Time &amp; Display</legend>

        <div class="field">
          <label for="tzPreset">Timezone Preset</label>
          <select id="tzPreset">
            <option value="">-- Select preset or type POSIX TZ below --</option>
            <option value="UTC0">UTC (UTC0)</option>
            <option value="NZST-12NZDT,M9.5.0,M4.1.0/3">New Zealand (Auckland)</option>
            <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia (Sydney/Melbourne)</option>
            <option value="JST-9">Japan (Tokyo)</option>
            <option value="GMT0BST,M3.5.0/1,M10.5.0">UK (London)</option>
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe (Berlin/Paris)</option>
            <option value="EST5EDT,M3.2.0,M11.1.0">US Eastern (New York)</option>
            <option value="CST6CDT,M3.2.0,M11.1.0">US Central (Chicago)</option>
            <option value="MST7MDT,M3.2.0,M11.1.0">US Mountain (Denver)</option>
            <option value="PST8PDT,M3.2.0,M11.1.0">US Pacific (Los Angeles)</option>
          </select>
        </div>

        <div class="field">
          <label for="timezone">POSIX Timezone String <span aria-hidden="true">*</span></label>
          <span id="tz-hint" class="hint">Used by ESP32 NTP for automatic daylight saving time.</span>
          <input type="text" id="timezone" name="timezone" required maxlength="48"
            autocorrect="off" autocapitalize="none" spellcheck="false" enterkeyhint="next"
            aria-describedby="tz-hint" data-error="tz-error">
          <div id="tz-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter a POSIX timezone string (e.g. UTC0).
          </div>
        </div>

        <div class="field">
          <label for="brightness">Screen Brightness (20&ndash;255)</label>
          <input type="number" id="brightness" name="brightness" min="20" max="255" required
            aria-describedby="bright-error" data-error="bright-error">
          <div id="bright-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter a brightness between 20 and 255.
          </div>
        </div>

        <div class="check-row">
          <input type="checkbox" id="use24Hour" name="use24Hour" value="true">
          <label for="use24Hour">Use 24-hour clock format</label>
        </div>
        <div class="check-row">
          <input type="checkbox" id="useFahrenheit" name="useFahrenheit" value="true">
          <label for="useFahrenheit">Show temperature in &deg;F (Fahrenheit)</label>
        </div>
        <div class="check-row">
          <input type="checkbox" id="useImperialDist" name="useImperialDist" value="true">
          <label for="useImperialDist">Use aviation/imperial flight units (ft, kt, mi)</label>
        </div>
      </fieldset>

      <fieldset style="margin-bottom:1.25rem;">
        <legend>Flight Radar</legend>

        <div class="field">
          <label for="searchRadiusKm">Detection Radius (km) <span aria-hidden="true">*</span></label>
          <span id="rad-hint" class="hint">Planes within this horizontal distance trigger the flight screen (1&ndash;150 km).</span>
          <input type="number" id="searchRadiusKm" name="searchRadiusKm" min="1" max="150" step="0.5" required
            aria-describedby="rad-hint" data-error="rad-error">
          <div id="rad-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter a radius between 1 and 150 km.
          </div>
        </div>

        <div class="field">
          <label for="flightPollSec">Poll Interval (seconds) <span aria-hidden="true">*</span></label>
          <span id="poll-hint" class="hint">How often to check for overhead flights (5&ndash;120 seconds).</span>
          <input type="number" id="flightPollSec" name="flightPollSec" min="5" max="120" required
            aria-describedby="poll-hint" data-error="poll-error">
          <div id="poll-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter an interval between 5 and 120 seconds.
          </div>
        </div>

        <div class="field">
          <label for="maxAltitudeFt">Max Altitude Filter (ft)</label>
          <input type="number" id="maxAltitudeFt" name="maxAltitudeFt" min="1000" max="60000" step="500" required
            data-error="alt-error">
          <div id="alt-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter an altitude between 1000 and 60000 ft.
          </div>
        </div>

        <div class="field">
          <label for="flightSource">ADS-B Data Source</label>
          <select id="flightSource" name="flightSource">
            <option value="airplanes.live">airplanes.live</option>
            <option value="adsb.lol">adsb.lol</option>
          </select>
        </div>
      </fieldset>

      <div class="actions">
        <button type="submit" id="btn-save" class="primary">Save &amp; Apply</button>
        <button type="button" id="btn-reboot">Reboot ESP32</button>
      </div>
    </form>
  </main>

  <script>
    const form = document.getElementById('config-form');
    const banner = document.getElementById('save-banner');

    // Consistent ARIA & :user-invalid fallback
    const linkError = (el, shown) => {
      const errorId = el.dataset.error;
      if (!errorId) return;
      const ids = (el.getAttribute('aria-describedby') || '').split(/\s+/).filter((id) => id && id !== errorId);
      if (shown) ids.push(errorId);
      if (ids.length) el.setAttribute('aria-describedby', ids.join(' '));
      else el.removeAttribute('aria-describedby');
    };

    const announcer = document.createElement('div');
    announcer.setAttribute('aria-live', 'polite');
    announcer.classList.add('visually-hidden');
    document.body.append(announcer);
    let announceTimer;

    const syncAria = (el, announce = false) => {
      if (!el.matches?.('input, select, textarea')) return;
      const supportsUserInvalid = CSS.supports('selector(:user-invalid)');
      const invalid = supportsUserInvalid ? el.matches(':user-invalid') : el.classList.contains('user-invalid-fallback');
      if (invalid) {
        el.setAttribute('aria-invalid', 'true');
        linkError(el, true);
        if (announce && el.dataset.error) {
          const errEl = document.getElementById(el.dataset.error);
          if (errEl) {
            clearTimeout(announceTimer);
            announceTimer = setTimeout(() => { announcer.textContent = errEl.textContent.trim(); }, 400);
          }
        }
      } else {
        el.setAttribute('aria-invalid', 'false');
        linkError(el, false);
      }
    };

    if (!CSS.supports('selector(:user-invalid)')) {
      const touched = new WeakSet();
      form.addEventListener('blur', (e) => {
        if (e.target.checkValidity) {
          touched.add(e.target);
          e.target.classList.toggle('user-invalid-fallback', !e.target.validity.valid);
          syncAria(e.target, true);
        }
      }, true);
      form.addEventListener('input', (e) => {
        if (touched.has(e.target) && e.target.checkValidity) {
          e.target.classList.toggle('user-invalid-fallback', !e.target.validity.valid);
          syncAria(e.target, false);
        }
      });
    } else {
      form.addEventListener('blur', (e) => syncAria(e.target, true), true);
      form.addEventListener('input', (e) => syncAria(e.target, false));
    }

    const showBanner = (msg, ok) => {
      banner.textContent = msg;
      banner.className = 'banner show ' + (ok ? 'ok' : 'err');
    };

    // Password toggle
    const passInput = document.getElementById('wifiPass');
    const togglePassBtn = document.getElementById('btn-toggle-pass');
    togglePassBtn.addEventListener('click', () => {
      const showing = passInput.type === 'text';
      passInput.type = showing ? 'password' : 'text';
      togglePassBtn.setAttribute('aria-pressed', String(!showing));
      togglePassBtn.textContent = showing ? 'Show' : 'Hide';
    });

    // Timezone preset helper
    document.getElementById('tzPreset').addEventListener('change', (e) => {
      if (e.target.value) {
        const tz = document.getElementById('timezone');
        tz.value = e.target.value;
        syncAria(tz, false);
      }
    });

    // Geolocation helper
    document.getElementById('btn-geo').addEventListener('click', () => {
      if (!navigator.geolocation) {
        showBanner('Browser geolocation is unavailable on this device.', false);
        return;
      }
      navigator.geolocation.getCurrentPosition(
        (pos) => {
          document.getElementById('latitude').value = pos.coords.latitude.toFixed(5);
          document.getElementById('longitude').value = pos.coords.longitude.toFixed(5);
          showBanner('Coordinates filled from browser GPS. Click Save & Apply.', true);
        },
        (err) => {
          showBanner('Could not get GPS (' + err.message + '). Enter coordinates manually.', false);
        },
        { enableHighAccuracy: true, timeout: 8000 }
      );
    });

    // Wi-Fi scan
    document.getElementById('btn-scan').addEventListener('click', async () => {
      const btn = document.getElementById('btn-scan');
      btn.disabled = true;
      btn.textContent = 'Scanning...';
      try {
        const res = await fetch('/api/scan');
        const nets = await res.json();
        const dl = document.getElementById('ssid-list');
        dl.innerHTML = '';
        nets.forEach((n) => {
          const opt = document.createElement('option');
          opt.value = n.ssid;
          opt.label = `${n.ssid} (${n.rssi} dBm)`;
          dl.appendChild(opt);
        });
        showBanner(`Found ${nets.length} Wi-Fi network(s). Select from the SSID field.`, true);
      } catch {
        showBanner('Wi-Fi scan failed.', false);
      } finally {
        btn.disabled = false;
        btn.textContent = 'Scan';
      }
    });

    // Load status & config
    async function loadStatus() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        document.getElementById('st-wifi').textContent = data.wifiConnected ? `${data.ssid} (${data.ip})` : `AP Mode (${data.ip})`;
        document.getElementById('st-time').textContent = data.time || '--:--';
        document.getElementById('st-weather').textContent = data.weather || '--';
        document.getElementById('st-flight').textContent = data.flight || 'None in range';
      } catch {
        document.getElementById('st-wifi').textContent = 'Offline';
      }
    }

    async function loadConfig() {
      try {
        const res = await fetch('/api/config');
        const c = await res.json();
        document.getElementById('wifiSsid').value = c.wifiSsid || '';
        document.getElementById('latitude').value = c.locationConfigured ? c.latitude : '';
        document.getElementById('longitude').value = c.locationConfigured ? c.longitude : '';
        document.getElementById('timezone').value = c.timezone || 'UTC0';
        document.getElementById('brightness').value = c.brightness ?? 220;
        document.getElementById('use24Hour').checked = !!c.use24Hour;
        document.getElementById('useFahrenheit').checked = !!c.useFahrenheit;
        document.getElementById('useImperialDist').checked = !!c.useImperialDist;
        document.getElementById('searchRadiusKm').value = c.searchRadiusKm ?? 15;
        document.getElementById('flightPollSec').value = c.flightPollSec ?? 15;
        document.getElementById('maxAltitudeFt').value = c.maxAltitudeFt ?? 50000;
        document.getElementById('flightSource').value = c.flightSource || 'airplanes.live';
      } catch {}
    }

    document.getElementById('btn-refresh').addEventListener('click', loadStatus);
    document.getElementById('btn-test-flight').addEventListener('click', async () => {
      await fetch('/api/test-flight', { method: 'POST' });
      showBanner('Triggered 10-second test flight display on screen.', true);
    });
    document.getElementById('btn-reboot').addEventListener('click', async () => {
      await fetch('/api/reboot', { method: 'POST' });
      showBanner('Rebooting ESP32-C3...', true);
    });

    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      if (!form.checkValidity()) {
        const firstInvalid = form.querySelector(':invalid');
        form.querySelectorAll('input, select').forEach((el) => {
          if (!el.validity.valid) {
            el.classList.add('user-invalid-fallback');
            syncAria(el, false);
          }
        });
        if (firstInvalid) firstInvalid.focus();
        showBanner('Please fix the highlighted fields before saving.', false);
        return;
      }

      const saveBtn = document.getElementById('btn-save');
      saveBtn.disabled = true;
      const body = new URLSearchParams(new FormData(form));
      // Ensure unchecked checkboxes are explicitly sent as false
      ['use24Hour', 'useFahrenheit', 'useImperialDist'].forEach((k) => {
        if (!document.getElementById(k).checked) body.set(k, 'false');
      });

      try {
        const res = await fetch('/api/config', { method: 'POST', body });
        const json = await res.json();
        showBanner(json.message || 'Settings saved!', res.ok);
        loadStatus();
      } catch {
        showBanner('Failed to save settings.', false);
      } finally {
        saveBtn.disabled = false;
      }
    });

    loadConfig();
    loadStatus();
    setInterval(loadStatus, 10000);
  </script>
</body>
</html>
)rawliteral";
