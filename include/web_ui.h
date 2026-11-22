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
      --slider-track: #73859e;
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
        --slider-track: #768599;
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
      padding-bottom: 5.5rem;
      display: flex;
      flex-direction: column;
      gap: 1rem;
    }
    .page-header {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: flex-start;
      gap: 0.75rem;
    }
    .btn-compact {
      min-height: 40px;
      padding: 0.4rem 0.75rem;
      font-size: 0.875rem;
    }
    [data-toggle-cards][aria-expanded="true"] .expand,
    [data-toggle-cards][aria-expanded="false"] .collapse {
      display: none;
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
    .card, details.card-details {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 0.75rem;
      margin: 0;
    }
    .card {
      padding: 1.125rem;
    }
    details.card-details {
      margin-bottom: 1rem;
    }
    details.card-details:last-of-type {
      margin-bottom: 0;
    }
    details.card-details > summary {
      list-style: none;
      display: flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.95rem 1.125rem;
      font-weight: 700;
      font-size: 1.02rem;
      color: var(--accent);
      cursor: pointer;
      border-radius: 0.75rem;
      user-select: none;
    }
    details.card-details > summary::-webkit-details-marker {
      display: none;
    }
    details.card-details > summary::before {
      content: "";
      inline-size: 1em;
      block-size: 1em;
      flex-shrink: 0;
      background-color: currentColor;
      -webkit-mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Cpath fill='black' d='M3.2 5.7a.75.75 0 0 1 1.06 0L8 9.44l3.74-3.74a.75.75 0 1 1 1.06 1.06l-4.27 4.27a.75.75 0 0 1-1.06 0L3.2 6.76a.75.75 0 0 1 0-1.06Z'/%3E%3C/svg%3E") no-repeat center / contain;
      mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Cpath fill='black' d='M3.2 5.7a.75.75 0 0 1 1.06 0L8 9.44l3.74-3.74a.75.75 0 1 1 1.06 1.06l-4.27 4.27a.75.75 0 0 1-1.06 0L3.2 6.76a.75.75 0 0 1 0-1.06Z'/%3E%3C/svg%3E") no-repeat center / contain;
      rotate: -90deg;
    }
    details.card-details[open] > summary::before {
      rotate: 0deg;
    }
    details.card-details > summary:focus-visible {
      outline: 3px solid var(--accent);
      outline-offset: 2px;
    }
    details.card-details::details-content {
      interpolate-size: allow-keywords;
      overflow: clip;
      contain: layout;
    }
    details.card-details:not([open])::details-content {
      block-size: 0;
    }
    @media (prefers-reduced-motion: no-preference) {
      details.card-details > summary::before {
        transition: rotate 0.2s ease;
      }
      details.card-details::details-content {
        transition: block-size, content-visibility, display;
        transition-duration: 0.25s;
        transition-timing-function: ease-in-out;
        transition-behavior: allow-discrete;
      }
    }
    .card-body {
      padding: 0 1.125rem 1.125rem;
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
    .toast-region {
      position: fixed;
      inset: 0.75rem 0.75rem auto 0.75rem;
      pointer-events: none;
      z-index: 60;
      display: flex;
      justify-content: center;
    }
    .toast {
      pointer-events: auto;
      display: none;
      align-items: center;
      justify-content: space-between;
      gap: 0.75rem;
      width: min(36rem, calc(100vw - 1.5rem));
      padding: 0.75rem 1rem;
      border-radius: 0.75rem;
      border: 1.5px solid var(--border);
      background: var(--card);
      color: var(--text);
      box-shadow: 0 10px 28px rgba(0, 0, 0, 0.35);
      margin: 0;
    }
    .toast[popover] {
      position: fixed;
      inset: 0.75rem 0.75rem auto 0.75rem;
      margin: 0 auto;
    }
    .toast.show,
    .toast[popover]:is(:popover-open, .\:popover-open) {
      display: flex;
    }
    .toast.ok { border-color: var(--ok); }
    .toast.err { border-color: var(--danger); }
    .toast-msg {
      flex: 1;
      min-width: 0;
      font-weight: 600;
      word-break: break-word;
    }
    .toast.ok .toast-msg { color: var(--ok); }
    .toast.err .toast-msg { color: var(--danger); }
    .toast-close {
      min-height: 36px;
      padding: 0.25rem 0.65rem;
      font-size: 0.875rem;
      flex-shrink: 0;
    }
    @media (prefers-reduced-motion: no-preference) {
      .toast[popover] {
        opacity: 0;
        transform: translateY(-0.5rem);
        transition: opacity 0.2s ease, transform 0.2s ease, display 0.2s, overlay 0.2s;
        transition-behavior: allow-discrete;
      }
      .toast[popover]:is(:popover-open, .\:popover-open) {
        opacity: 1;
        transform: translateY(0);
      }
      @starting-style {
        .toast[popover]:popover-open {
          opacity: 0;
          transform: translateY(-0.5rem);
        }
      }
    }
    .floating-bar {
      position: fixed;
      inset: auto 0 0 0;
      z-index: 40;
      padding: 0.75rem 1rem;
      background: var(--card);
      border-top: 1.5px solid var(--accent);
      box-shadow: 0 -8px 24px rgba(0, 0, 0, 0.32);
    }
    .floating-bar[hidden] {
      display: none;
    }
    .floating-bar-inner {
      max-width: 36rem;
      margin: 0 auto;
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 0.75rem;
    }
    .floating-bar-label {
      font-weight: 600;
      font-size: 0.95rem;
      color: var(--accent);
    }
    .floating-bar-actions {
      display: flex;
      gap: 0.5rem;
      flex: 1 1 14rem;
      justify-content: flex-end;
    }
    .floating-bar-actions button {
      min-height: 44px;
      flex: 1 1 auto;
    }
    progress {
      width: 100%;
      height: 0.75rem;
      accent-color: var(--accent);
    }
    input[type="file"] {
      width: 100%;
      padding: 0.5rem;
      background: var(--bg);
      color: var(--text);
      border: 1.5px dashed var(--border);
    }
    .range-header {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      gap: 0.5rem;
    }
    .range-value {
      font-weight: 700;
      font-variant-numeric: tabular-nums;
      color: var(--accent);
    }
    input[type="range"] {
      --slider-track-size: 0.5rem;
      --slider-thumb-size: 1.25rem;
      --slider-track-radius: 999px;
      --slider-thumb-radius: 50%;
      --slider-track-color: var(--slider-track);
      --slider-fill-color: var(--accent);
      --slider-thumb-color: var(--slider-fill-color);
      --slider-direction: to right;
      --attr-min: attr(min type(<number>), 5);
      --attr-max: attr(max type(<number>), 20);
      --control-value: attr(value type(<number>), 5);
      --slider-progress: calc(
        progress(var(--control-value), var(--attr-min), var(--attr-max)) * 100%
      );

      appearance: none;
      inline-size: 100%;
      block-size: 3rem;
      margin: 0;
      padding: 0;
      border: 0;
      border-radius: 0.5rem;
      background: transparent;
      cursor: pointer;
    }
    input[type="range"]:dir(rtl) {
      --slider-direction: to left;
    }
    @supports not (opacity: progress(1, 0, 100)) {
      input[type="range"] {
        --slider-progress: calc(
          (var(--control-value) - var(--attr-min)) /
          (var(--attr-max) - var(--attr-min)) * 100%
        );
      }
    }
    input[type="range"]::-webkit-slider-runnable-track {
      block-size: var(--slider-track-size);
      border: 0;
      border-radius: var(--slider-track-radius);
      background: linear-gradient(
        var(--slider-direction),
        var(--slider-fill-color) var(--slider-progress),
        var(--slider-track-color) var(--slider-progress)
      );
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      inline-size: var(--slider-thumb-size);
      block-size: var(--slider-thumb-size);
      margin-block-start: calc((var(--slider-track-size) - var(--slider-thumb-size)) / 2);
      border: 0;
      border-radius: var(--slider-thumb-radius);
      background: var(--slider-thumb-color);
    }
    input[type="range"]::-moz-range-track {
      block-size: var(--slider-track-size);
      border: 0;
      border-radius: var(--slider-track-radius);
      background: var(--slider-track-color);
    }
    input[type="range"]::-moz-range-progress {
      block-size: var(--slider-track-size);
      border-radius: var(--slider-track-radius);
      background: var(--slider-fill-color);
    }
    input[type="range"]::-moz-range-thumb {
      inline-size: var(--slider-thumb-size);
      block-size: var(--slider-thumb-size);
      border: 0;
      border-radius: var(--slider-thumb-radius);
      background: var(--slider-thumb-color);
    }
    input[type="range"]:focus-visible {
      outline: 2px solid var(--slider-fill-color);
      outline-offset: 4px;
    }
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
    <header class="page-header">
      <div>
        <h1>ESP32 Flight Clock</h1>
        <p class="subtitle">1.28&Prime; Round TFT &bull; Time, Weather &amp; Overhead Flight Radar</p>
      </div>
      <button type="button" id="btn-toggle-cards" class="btn-compact" data-toggle-cards aria-expanded="true">
        <span class="expand">Expand all</span>
        <span class="collapse">Collapse all</span>
      </button>
    </header>

    <details class="card-details" open>
      <summary>Live Status</summary>
      <div class="card-body">
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
      </div>
    </details>

    <form id="config-form" method="POST" action="/api/config" aria-labelledby="form-heading" novalidate>
      <h2 id="form-heading" class="visually-hidden">Device Configuration</h2>

      <details class="card-details" open>
        <summary>Wi-Fi Connectivity</summary>
        <div class="card-body">

        <div class="field">
          <label for="wifiSsid">Wi-Fi Network (SSID) <span aria-hidden="true">*</span></label>
          <span id="ssid-hint" class="hint">2.4 GHz Wi-Fi network name. Use Scan to pick a nearby network.</span>
          <div class="input-row">
            <input type="text" id="wifiSsid" name="wifiSsid" required maxlength="32"
              autocorrect="off" autocapitalize="none" spellcheck="false" enterkeyhint="next"
              aria-describedby="ssid-hint" data-error="ssid-error">
            <button type="button" id="btn-scan">Scan</button>
          </div>
          <div id="ssid-error" class="error-msg">
            <span role="img" aria-label="Error">&#10060;</span> Enter a Wi-Fi network name (1&ndash;32 characters).
          </div>
          <select id="ssid-select" aria-label="Scanned Wi-Fi networks" style="margin-top:0.35rem;" hidden></select>
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

        <div class="field">
          <div class="range-header">
            <label for="wifiTxPowerDbm">Wi-Fi TX Power</label>
            <output for="wifiTxPowerDbm" id="txPower-val" class="range-value">5.0 dBm</output>
          </div>
          <span id="txpower-hint" class="hint">Transmit power from 5.0 to 20.0 dBm (0.5 dBm steps). Default 5.0 dBm prevents RF brownout on ESP32-C3 Super Mini; increase if signal is weak.</span>
          <input type="range" id="wifiTxPowerDbm" name="wifiTxPowerDbm" min="5" max="20" step="0.5" value="5"
            aria-describedby="txpower-hint" aria-valuetext="5.0 dBm">
        </div>
        </div>
      </details>

      <details class="card-details" open>
        <summary>GPS Location</summary>
        <div class="card-body">

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
        </div>
      </details>

      <details class="card-details" open>
        <summary>Time &amp; Display</summary>
        <div class="card-body">

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
        </div>
      </details>

      <details class="card-details" open>
        <summary>Flight Radar</summary>
        <div class="card-body">

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
            <option value="adsb.lol">adsb.lol</option>
            <option value="airplanes.live">airplanes.live</option>
          </select>
        </div>

        <div class="check-row">
          <input type="checkbox" id="quietHoursEnabled" name="quietHoursEnabled">
          <label for="quietHoursEnabled">Pause flight polling during quiet hours</label>
        </div>

        <div class="field">
          <label>Quiet Hours (24h, local time)</label>
          <span id="quiet-hint" class="hint">Start and end hour (0&ndash;23). Example: 1 to 6 pauses 1:00 AM&ndash;6:00 AM; 23 to 6 pauses overnight.</span>
          <div class="input-row">
            <input type="number" id="quietStartHour" name="quietStartHour" min="0" max="23" required
              aria-label="Quiet start hour" aria-describedby="quiet-hint">
            <input type="number" id="quietEndHour" name="quietEndHour" min="0" max="23" required
              aria-label="Quiet end hour" aria-describedby="quiet-hint">
          </div>
        </div>
        </div>
      </details>

      <div class="actions" style="margin-top:1rem;">
        <button type="submit" id="btn-save" class="primary">Save &amp; Apply</button>
        <button type="button" id="btn-reboot">Reboot ESP32</button>
      </div>
    </form>

    <details class="card-details" open>
      <summary>Firmware Update (OTA)</summary>
      <div class="card-body">
        <p class="hint" style="margin:0 0 0.875rem;">
          Current build: <strong id="st-fw">--</strong>. Upload a compiled <code>firmware_v*.bin</code> file to update wirelessly, or run <code>pio run -e esp32-c3-supermini-ota -t upload</code>.
        </p>
        <div class="field">
          <label for="ota-file">Firmware Binary (.bin)</label>
          <input type="file" id="ota-file" accept=".bin,application/octet-stream">
        </div>
        <div class="field" id="ota-progress-wrap" hidden>
          <progress id="ota-progress" max="100" value="0"></progress>
          <span id="ota-status" class="hint" role="status" aria-live="polite"></span>
        </div>
        <div class="actions">
          <button type="button" id="btn-ota">Upload Firmware</button>
        </div>
      </div>
    </details>
  </main>

  <div role="region" aria-label="Notifications" class="toast-region">
    <div id="save-toast" class="toast" popover="manual">
      <span id="toast-msg" class="toast-msg"></span>
      <button type="button" id="btn-toast-close" class="toast-close" popovertarget="save-toast" popovertargetaction="hide" aria-label="Dismiss notification">Close</button>
    </div>
  </div>
  <div id="toast-live" class="visually-hidden" aria-live="polite" aria-atomic="true"></div>

  <div id="unsaved-bar" class="floating-bar" role="region" aria-label="Unsaved changes" hidden>
    <div class="floating-bar-inner">
      <span class="floating-bar-label">Unsaved changes</span>
      <div class="floating-bar-actions">
        <button type="button" id="btn-cancel">Cancel</button>
        <button type="submit" form="config-form" id="btn-float-save" class="primary">Save &amp; Apply</button>
      </div>
    </div>
  </div>

  <script>
    const form = document.getElementById('config-form');
    const toast = document.getElementById('save-toast');
    const toastMsg = document.getElementById('toast-msg');
    const toastCloseBtn = document.getElementById('btn-toast-close');
    const toastLive = document.getElementById('toast-live');
    const unsavedBar = document.getElementById('unsaved-bar');
    const supportsPopover = Object.hasOwn(HTMLElement.prototype, 'popover');

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

    // Viewport-fixed confirmation toast (Top Layer via popover="manual" with fixed fallback)
    let toastTimer = null;
    let toastAutoDismiss = false;
    let lastFocusBeforeToast = null;

    const isToastOpen = () => (supportsPopover ? toast.matches(':popover-open') : toast.classList.contains('show'));

    const hideToast = () => {
      clearTimeout(toastTimer);
      toastTimer = null;
      const hadFocus = toast.contains(document.activeElement);
      if (supportsPopover) {
        if (toast.matches(':popover-open')) toast.hidePopover();
      } else {
        toast.classList.remove('show');
      }
      if (hadFocus && lastFocusBeforeToast && typeof lastFocusBeforeToast.focus === 'function') {
        lastFocusBeforeToast.focus();
      }
    };

    const scheduleToastDismiss = () => {
      clearTimeout(toastTimer);
      if (!toastAutoDismiss) return;
      toastTimer = setTimeout(() => {
        if (toast.contains(document.activeElement)) return;
        hideToast();
      }, 5000);
    };

    toast.addEventListener('focusin', () => clearTimeout(toastTimer));
    toast.addEventListener('focusout', (e) => {
      if (!toast.contains(e.relatedTarget) && isToastOpen()) scheduleToastDismiss();
    });
    if (supportsPopover) {
      toast.addEventListener('toggle', (e) => {
        if (e.newState === 'closed') {
          clearTimeout(toastTimer);
          toastTimer = null;
        }
      });
      toastCloseBtn.addEventListener('click', () => {
        setTimeout(() => {
          if (lastFocusBeforeToast && typeof lastFocusBeforeToast.focus === 'function') {
            lastFocusBeforeToast.focus();
          }
        }, 0);
      });
    } else {
      toastCloseBtn.addEventListener('click', hideToast);
    }

    const showBanner = (msg, ok) => {
      if (!toast.contains(document.activeElement)) {
        lastFocusBeforeToast = document.activeElement;
      }
      toastMsg.textContent = msg;
      toast.classList.remove('ok', 'err');
      toast.classList.add(ok ? 'ok' : 'err');
      toastAutoDismiss = !!ok;

      if (supportsPopover) {
        if (!toast.matches(':popover-open')) toast.showPopover();
      } else {
        toast.classList.add('show');
      }

      if (typeof document.ariaNotify === 'function') {
        document.ariaNotify(msg);
      } else {
        toastLive.textContent = '';
        setTimeout(() => { toastLive.textContent = msg; }, 50);
      }
      scheduleToastDismiss();
    };

    // Expand / Collapse all cards
    const toggleCardsBtn = document.getElementById('btn-toggle-cards');
    const getCardDetails = () => Array.from(document.querySelectorAll('details.card-details'));
    const syncToggleCardsBtn = () => {
      const allOpen = getCardDetails().every((d) => d.open);
      toggleCardsBtn.setAttribute('aria-expanded', String(allOpen));
    };
    toggleCardsBtn.addEventListener('click', () => {
      const shouldOpen = toggleCardsBtn.getAttribute('aria-expanded') !== 'true';
      getCardDetails().forEach((d) => { d.open = shouldOpen; });
      syncToggleCardsBtn();
    });
    document.addEventListener('toggle', (e) => {
      if (e.target instanceof HTMLDetailsElement && e.target.classList.contains('card-details')) {
        syncToggleCardsBtn();
      }
    }, { capture: true });

    // Unsaved-changes tracking for floating Save / Cancel bar
    const trackedTextFields = [
      'wifiSsid', 'wifiPass', 'wifiTxPowerDbm', 'latitude', 'longitude',
      'timezone', 'brightness', 'searchRadiusKm', 'flightPollSec',
      'maxAltitudeFt', 'flightSource', 'quietStartHour', 'quietEndHour'
    ];
    const trackedBoolFields = [
      'use24Hour', 'useFahrenheit', 'useImperialDist', 'quietHoursEnabled'
    ];
    let savedSnapshot = null;

    const captureConfigSnapshot = () => {
      const snap = {};
      trackedTextFields.forEach((id) => {
        const el = document.getElementById(id);
        snap[id] = el ? el.value : '';
      });
      trackedBoolFields.forEach((id) => {
        const el = document.getElementById(id);
        snap[id] = !!(el && el.checked);
      });
      return snap;
    };

    const isFormDirty = () => {
      if (!savedSnapshot) return false;
      const cur = captureConfigSnapshot();
      for (const k of trackedTextFields) {
        if (cur[k] !== savedSnapshot[k]) return true;
      }
      for (const k of trackedBoolFields) {
        if (cur[k] !== savedSnapshot[k]) return true;
      }
      return false;
    };

    const updateDirtyState = () => {
      unsavedBar.hidden = !isFormDirty();
    };

    form.addEventListener('input', updateDirtyState);
    form.addEventListener('change', updateDirtyState);

    document.getElementById('btn-cancel').addEventListener('click', () => {
      if (!savedSnapshot) return;
      trackedTextFields.forEach((id) => {
        const el = document.getElementById(id);
        if (el) {
          el.value = savedSnapshot[id];
          el.classList.remove('user-invalid-fallback');
          syncAria(el, false);
        }
      });
      trackedBoolFields.forEach((id) => {
        const el = document.getElementById(id);
        if (el) el.checked = !!savedSnapshot[id];
      });
      document.getElementById('tzPreset').value = '';
      syncTxPowerSlider();
      updateDirtyState();
      showBanner('Changes discarded.', true);
    });

    // Password toggle
    const passInput = document.getElementById('wifiPass');
    const togglePassBtn = document.getElementById('btn-toggle-pass');
    togglePassBtn.addEventListener('click', () => {
      const showing = passInput.type === 'text';
      passInput.type = showing ? 'password' : 'text';
      togglePassBtn.setAttribute('aria-pressed', String(!showing));
      togglePassBtn.textContent = showing ? 'Show' : 'Hide';
    });

    // Wi-Fi TX Power slider sync
    const txPowerInput = document.getElementById('wifiTxPowerDbm');
    const txPowerOut = document.getElementById('txPower-val');
    function syncTxPowerSlider() {
      const val = parseFloat(txPowerInput.value);
      const formatted = `${Number.isFinite(val) ? val.toFixed(1) : '5.0'} dBm`;
      txPowerInput.style.setProperty('--attr-min', txPowerInput.min || 5);
      txPowerInput.style.setProperty('--attr-max', txPowerInput.max || 20);
      txPowerInput.style.setProperty('--control-value', txPowerInput.value);
      txPowerInput.setAttribute('aria-valuetext', formatted);
      txPowerOut.textContent = formatted;
    }
    syncTxPowerSlider();
    txPowerInput.addEventListener('input', syncTxPowerSlider);

    // Timezone preset helper
    document.getElementById('tzPreset').addEventListener('change', (e) => {
      if (e.target.value) {
        const tz = document.getElementById('timezone');
        tz.value = e.target.value;
        syncAria(tz, false);
        updateDirtyState();
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
          updateDirtyState();
          showBanner('Coordinates filled from browser GPS. Click Save & Apply.', true);
        },
        (err) => {
          showBanner('Could not get GPS (' + err.message + '). Enter coordinates manually.', false);
        },
        { enableHighAccuracy: true, timeout: 8000 }
      );
    });

    // Wi-Fi scan
    const ssidSelect = document.getElementById('ssid-select');
    ssidSelect.addEventListener('change', (e) => {
      if (e.target.value) {
        const ssidInput = document.getElementById('wifiSsid');
        ssidInput.value = e.target.value;
        syncAria(ssidInput, false);
        updateDirtyState();
        document.getElementById('wifiPass').focus();
      }
    });

    document.getElementById('btn-scan').addEventListener('click', async () => {
      const btn = document.getElementById('btn-scan');
      btn.disabled = true;
      btn.textContent = 'Scanning...';
      try {
        const res = await fetch('/api/scan');
        const nets = await res.json();
        ssidSelect.innerHTML = '';
        if (nets.length > 0) {
          const placeholder = document.createElement('option');
          placeholder.value = '';
          placeholder.textContent = `-- Select a network (${nets.length} found) --`;
          ssidSelect.appendChild(placeholder);
          nets.forEach((n) => {
            const opt = document.createElement('option');
            opt.value = n.ssid;
            opt.textContent = `${n.ssid} (${n.rssi} dBm)`;
            ssidSelect.appendChild(opt);
          });
          ssidSelect.hidden = false;
          ssidSelect.focus();
          showBanner(`Found ${nets.length} Wi-Fi network(s). Pick one from the dropdown below.`, true);
        } else {
          ssidSelect.hidden = true;
          showBanner('No 2.4 GHz Wi-Fi networks found. Try again or type the SSID manually.', false);
        }
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
        if (data.firmware) document.getElementById('st-fw').textContent = data.firmware;
      } catch {
        document.getElementById('st-wifi').textContent = 'Offline';
      }
    }

    async function loadConfig() {
      try {
        const res = await fetch('/api/config');
        const c = await res.json();
        document.getElementById('wifiSsid').value = c.wifiSsid || '';
        txPowerInput.value = c.wifiTxPowerDbm ?? 5;
        syncTxPowerSlider();
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
        document.getElementById('quietHoursEnabled').checked = c.quietHoursEnabled !== false;
        document.getElementById('quietStartHour').value = c.quietStartHour ?? 1;
        document.getElementById('quietEndHour').value = c.quietEndHour ?? 6;
        savedSnapshot = captureConfigSnapshot();
        updateDirtyState();
      } catch {
        savedSnapshot = captureConfigSnapshot();
        updateDirtyState();
      }
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

    // Web OTA firmware upload
    document.getElementById('btn-ota').addEventListener('click', () => {
      const fileInput = document.getElementById('ota-file');
      const file = fileInput.files && fileInput.files[0];
      if (!file) {
        showBanner('Select a firmware.bin file first.', false);
        return;
      }
      if (!file.name.toLowerCase().endsWith('.bin')) {
        showBanner('Please choose a compiled .bin firmware file.', false);
        return;
      }

      const btn = document.getElementById('btn-ota');
      const wrap = document.getElementById('ota-progress-wrap');
      const prog = document.getElementById('ota-progress');
      const status = document.getElementById('ota-status');

      btn.disabled = true;
      wrap.hidden = false;
      prog.value = 0;
      status.textContent = `Uploading ${file.name} (${(file.size / 1024).toFixed(0)} KB)...`;

      const formData = new FormData();
      formData.append('firmware', file, file.name);

      const xhr = new XMLHttpRequest();
      xhr.open('POST', `/api/ota?size=${encodeURIComponent(file.size)}`, true);

      xhr.upload.addEventListener('progress', (ev) => {
        if (ev.lengthComputable) {
          const pct = Math.round((ev.loaded / ev.total) * 100);
          prog.value = pct;
          status.textContent = pct < 100 ? `Uploading & flashing... ${pct}%` : 'Verifying & rebooting...';
        }
      });

      xhr.addEventListener('load', () => {
        let msg = 'Firmware updated! Rebooting...';
        let ok = xhr.status >= 200 && xhr.status < 300;
        try {
          const res = JSON.parse(xhr.responseText);
          if (res.message) msg = res.message;
          if (typeof res.ok === 'boolean') ok = res.ok;
        } catch {}
        status.textContent = msg;
        showBanner(msg, ok);
        if (ok) {
          prog.value = 100;
          setTimeout(() => location.reload(), 6000);
        } else {
          btn.disabled = false;
        }
      });

      xhr.addEventListener('error', () => {
        status.textContent = 'Upload failed (connection lost).';
        showBanner('Firmware upload failed. Check Wi-Fi connection and retry.', false);
        btn.disabled = false;
      });

      xhr.send(formData);
    });

    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      if (!form.checkValidity()) {
        const firstInvalid = form.querySelector(':invalid');
        form.querySelectorAll('input, select').forEach((el) => {
          if (!el.validity.valid) {
            const parentCard = el.closest('details.card-details');
            if (parentCard) parentCard.open = true;
            el.classList.add('user-invalid-fallback');
            syncAria(el, false);
          }
        });
        syncToggleCardsBtn();
        if (firstInvalid) firstInvalid.focus();
        showBanner('Please fix the highlighted fields before saving.', false);
        return;
      }

      const saveBtn = document.getElementById('btn-save');
      const floatSaveBtn = document.getElementById('btn-float-save');
      saveBtn.disabled = true;
      floatSaveBtn.disabled = true;
      const body = new URLSearchParams(new FormData(form));
      // Ensure unchecked checkboxes are explicitly sent as false
      ['use24Hour', 'useFahrenheit', 'useImperialDist', 'quietHoursEnabled'].forEach((k) => {
        if (!document.getElementById(k).checked) body.set(k, 'false');
      });

      try {
        const res = await fetch('/api/config', { method: 'POST', body });
        const json = await res.json();
        showBanner(json.message || 'Settings saved!', res.ok);
        if (res.ok) {
          passInput.value = '';
          savedSnapshot = captureConfigSnapshot();
          updateDirtyState();
        }
        loadStatus();
      } catch {
        showBanner('Failed to save settings.', false);
      } finally {
        saveBtn.disabled = false;
        floatSaveBtn.disabled = false;
      }
    });

    loadConfig();
    loadStatus();
    setInterval(loadStatus, 10000);
  </script>
</body>
</html>
)rawliteral";
