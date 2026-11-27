# ESP32-C3 Flight Clock — Docker Web Flasher

A lightweight, zero-dependency Docker web app that lets anyone flash an ESP32-C3 Super Mini directly from Chrome, Edge, or Opera using **Web Serial** (`esp-web-tools`), pulling firmware and release notes directly from GitHub Releases.

## Features

- **Branch & Version Dropdowns**: Groups GitHub Releases by branch (`main`, `beta`, `dev`, etc.) and version tag, defaulting to the latest build on `main`.
- **Release Notes Viewer**: Displays each release's notes safely in the browser.
- **CORS-Free Firmware Proxy**: Proxies `.bin` downloads server-side from GitHub Releases so ESP Web Tools works cleanly without CORS errors, and also provides direct **OTA `.bin`** and **Factory `.bin`** download links.
- **In-Memory Caching**: Caches GitHub API release metadata for 5 minutes (configurable via `CACHE_TTL_SECONDS`) with a manual **Refresh** button.

## Quick Start (Docker Compose)

```bash
cd web-flasher
cp .env.example .env
docker compose up -d --build
```

The flasher listens on port **`8080`** (`http://localhost:8080`).

## Publishing on Your Home Server (HTTPS Required)

> **Important:** Browsers restrict the **Web Serial API** to **HTTPS** (or `http://localhost`). When hosting on a public domain or LAN hostname, put a TLS reverse proxy in front of port `8080` (for example **Caddy**, **Nginx Proxy Manager**, **Traefik**, or **Cloudflare Tunnel**).

Example **Caddyfile**:

```caddy
flash.yourdomain.com {
    reverse_proxy flightclock-web-flasher:8080
}
```

## Environment Variables

| Variable | Default | Description |
| :--- | :--- | :--- |
| `PORT` | `8080` | HTTP port inside the container |
| `GITHUB_REPO` | `Redalin/esp32FlightClock` | GitHub `owner/repo` to pull releases from |
| `DEFAULT_BRANCH` | `main` | Branch pre-selected in the dropdown |
| `CACHE_TTL_SECONDS` | `300` | Seconds to cache GitHub release metadata |
| `APP_TITLE` | `ESP32-C3 Flight Clock` | Title displayed in the header and manifest |
| `GITHUB_TOKEN` | *(empty)* | Optional read-only GitHub PAT (raises API limit from 60/hr to 5,000/hr and enables private repos) |

## Running Tests Locally

```bash
node --test web-flasher/server.test.mjs
```
