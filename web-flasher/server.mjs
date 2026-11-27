import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PUBLIC_DIR = path.join(__dirname, "public");

const MIME_TYPES = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".ico": "image/x-icon",
};

const BRANCH_PRIORITY = new Map([
  ["main", 0],
  ["master", 1],
  ["beta", 2],
  ["dev", 3],
]);

export function selectFirmwareAssets(assets = []) {
  let factoryAsset = null;
  let otaAsset = null;
  let checksumAsset = null;

  for (const asset of assets) {
    if (!asset?.name) continue;
    const lower = asset.name.toLowerCase();
    if (lower === "sha256sums.txt" || lower.endsWith(".sha256")) {
      checksumAsset = {
        name: asset.name,
        size: asset.size ?? 0,
        downloadUrl: asset.browser_download_url,
        apiUrl: asset.url,
      };
    } else if (
      lower.endsWith("-factory.bin") ||
      (lower.includes("merged") && lower.endsWith(".bin"))
    ) {
      factoryAsset = {
        name: asset.name,
        size: asset.size ?? 0,
        downloadUrl: asset.browser_download_url,
        apiUrl: asset.url,
      };
    } else if (
      lower.endsWith("-ota.bin") ||
      (lower.startsWith("firmware") && lower.endsWith(".bin"))
    ) {
      otaAsset = {
        name: asset.name,
        size: asset.size ?? 0,
        downloadUrl: asset.browser_download_url,
        apiUrl: asset.url,
      };
    }
  }

  return { factoryAsset, otaAsset, checksumAsset };
}

export function normalizeBranch(targetCommitish, defaultBranch = "main") {
  if (!targetCommitish || /^[0-9a-f]{40}$/i.test(targetCommitish.trim())) {
    return defaultBranch;
  }
  return targetCommitish.trim();
}

export function groupReleasesByBranch(rawReleases = [], defaultBranch = "main") {
  const releasesByBranch = {};
  const releasesByTag = new Map();

  for (const rel of rawReleases) {
    if (!rel || rel.draft || !rel.tag_name) continue;
    const branch = normalizeBranch(rel.target_commitish, defaultBranch);
    const { factoryAsset, otaAsset, checksumAsset } = selectFirmwareAssets(rel.assets);

    const entry = {
      tag: rel.tag_name,
      name: rel.name || rel.tag_name,
      branch,
      prerelease: Boolean(rel.prerelease),
      publishedAt: rel.published_at || rel.created_at || "",
      htmlUrl: rel.html_url || "",
      notes: rel.body || "",
      factoryAsset: factoryAsset ? { name: factoryAsset.name, size: factoryAsset.size } : null,
      otaAsset: otaAsset ? { name: otaAsset.name, size: otaAsset.size } : null,
      checksumAsset: checksumAsset ? { name: checksumAsset.name, size: checksumAsset.size } : null,
      flashable: Boolean(factoryAsset),
      _rawAssets: { factoryAsset, otaAsset, checksumAsset },
    };

    if (!releasesByBranch[branch]) {
      releasesByBranch[branch] = [];
    }
    releasesByBranch[branch].push(entry);
    releasesByTag.set(entry.tag, entry);
  }

  for (const branch of Object.keys(releasesByBranch)) {
    releasesByBranch[branch].sort((a, b) =>
      (b.publishedAt || "").localeCompare(a.publishedAt || "")
    );
  }

  const branches = Object.keys(releasesByBranch).sort((a, b) => {
    const pa = a === defaultBranch ? -1 : (BRANCH_PRIORITY.get(a) ?? 100);
    const pb = b === defaultBranch ? -1 : (BRANCH_PRIORITY.get(b) ?? 100);
    if (pa !== pb) return pa - pb;
    return a.localeCompare(b);
  });

  return { branches, releasesByBranch, releasesByTag };
}

export function buildManifest(release, appTitle = "ESP32-C3 Flight Clock") {
  if (!release?.factoryAsset?.name) {
    return null;
  }
  const encodedTag = encodeURIComponent(release.tag);
  const encodedFile = encodeURIComponent(release.factoryAsset.name);
  return {
    name: appTitle,
    version: release.tag,
    new_install_prompt_erase: true,
    builds: [
      {
        chipFamily: "ESP32-C3",
        parts: [
          {
            path: `/api/firmware/${encodedTag}/${encodedFile}`,
            offset: 0,
          },
        ],
      },
    ],
  };
}

function stripInternalFields(grouped) {
  const cleanByBranch = {};
  for (const [branch, list] of Object.entries(grouped.releasesByBranch)) {
    cleanByBranch[branch] = list.map(({ _rawAssets, ...pub }) => pub);
  }
  return {
    branches: grouped.branches,
    releasesByBranch: cleanByBranch,
  };
}

function sendJson(res, statusCode, payload, extraHeaders = {}) {
  const body = JSON.stringify(payload);
  res.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
    ...extraHeaders,
  });
  res.end(body);
}

export function createFlasherServer(options = {}) {
  const repo = options.repo || process.env.GITHUB_REPO || "Redalin/esp32FlightClock";
  const token = options.token ?? process.env.GITHUB_TOKEN ?? "";
  const defaultBranch = options.defaultBranch || process.env.DEFAULT_BRANCH || "main";
  const appTitle = options.appTitle || process.env.APP_TITLE || "ESP32-C3 Flight Clock";
  const cacheTtlMs =
    Number(options.cacheTtlSeconds ?? process.env.CACHE_TTL_SECONDS ?? 300) * 1000;
  const fetchImpl = options.fetchImpl || globalThis.fetch;

  let cached = null;
  let cachedAt = 0;

  function githubHeaders(accept = "application/vnd.github+json") {
    const headers = {
      Accept: accept,
      "User-Agent": "esp32-flightclock-web-flasher",
      "X-GitHub-Api-Version": "2022-11-28",
    };
    if (token) {
      headers.Authorization = `Bearer ${token}`;
    }
    return headers;
  }

  async function getReleases(forceRefresh = false) {
    const now = Date.now();
    if (!forceRefresh && cached && now - cachedAt < cacheTtlMs) {
      return cached;
    }

    const url = `https://api.github.com/repos/${repo}/releases?per_page=100`;
    const response = await fetchImpl(url, { headers: githubHeaders() });
    if (!response.ok) {
      const text = await response.text().catch(() => "");
      const err = new Error(`GitHub API error (${response.status}): ${text.slice(0, 200)}`);
      err.statusCode = response.status;
      throw err;
    }

    const rawReleases = await response.json();
    cached = {
      fetchedAt: new Date(now).toISOString(),
      ...groupReleasesByBranch(rawReleases, defaultBranch),
    };
    cachedAt = now;
    return cached;
  }

  async function serveStatic(reqPath, res) {
    const relPath = reqPath === "/" ? "index.html" : reqPath.replace(/^\/+/, "");
    const resolved = path.resolve(PUBLIC_DIR, relPath);
    if (!resolved.startsWith(PUBLIC_DIR + path.sep) && resolved !== PUBLIC_DIR) {
      res.writeHead(403);
      res.end("Forbidden");
      return;
    }

    try {
      const stat = await fs.promises.stat(resolved);
      if (!stat.isFile()) {
        res.writeHead(404);
        res.end("Not found");
        return;
      }
      const ext = path.extname(resolved).toLowerCase();
      res.writeHead(200, {
        "Content-Type": MIME_TYPES[ext] || "application/octet-stream",
        "Cache-Control": ext === ".html" ? "no-cache" : "public, max-age=3600",
      });
      fs.createReadStream(resolved).pipe(res);
    } catch {
      res.writeHead(404);
      res.end("Not found");
    }
  }

  return http.createServer(async (req, res) => {
    try {
      const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
      const pathname = url.pathname;

      if (req.method !== "GET" && req.method !== "HEAD") {
        res.writeHead(405);
        res.end("Method Not Allowed");
        return;
      }

      if (pathname === "/health") {
        sendJson(res, 200, { status: "ok", repo });
        return;
      }

      if (pathname === "/api/config") {
        sendJson(res, 200, { repo, defaultBranch, appTitle });
        return;
      }

      if (pathname === "/api/releases") {
        const forceRefresh = url.searchParams.get("refresh") === "1";
        const data = await getReleases(forceRefresh);
        sendJson(res, 200, {
          repo,
          defaultBranch,
          appTitle,
          fetchedAt: data.fetchedAt,
          ...stripInternalFields(data),
        });
        return;
      }

      const manifestMatch = pathname.match(/^\/api\/manifest\/([^/]+)$/);
      if (manifestMatch) {
        const tag = decodeURIComponent(manifestMatch[1]);
        const data = await getReleases(false);
        const release = data.releasesByTag.get(tag);
        if (!release) {
          sendJson(res, 404, { error: `Release '${tag}' not found` });
          return;
        }
        const manifest = buildManifest(release, appTitle);
        if (!manifest) {
          sendJson(res, 404, { error: `Release '${tag}' has no factory firmware binary` });
          return;
        }
        sendJson(res, 200, manifest, { "Access-Control-Allow-Origin": "*" });
        return;
      }

      const fwMatch = pathname.match(/^\/api\/firmware\/([^/]+)\/([^/]+)$/);
      if (fwMatch) {
        const tag = decodeURIComponent(fwMatch[1]);
        const filename = decodeURIComponent(fwMatch[2]);
        const data = await getReleases(false);
        const release = data.releasesByTag.get(tag);
        if (!release) {
          sendJson(res, 404, { error: `Release '${tag}' not found` });
          return;
        }

        const allowed = Object.values(release._rawAssets).filter(Boolean);
        const asset = allowed.find((a) => a.name === filename);
        if (!asset) {
          sendJson(res, 404, { error: `Asset '${filename}' not found in release '${tag}'` });
          return;
        }

        // Use the GitHub API asset endpoint when a token is configured (supports private repos),
        // otherwise fetch the public browser_download_url directly.
        const upstreamUrl = token && asset.apiUrl ? asset.apiUrl : asset.downloadUrl;
        const upstreamHeaders =
          token && asset.apiUrl
            ? githubHeaders("application/octet-stream")
            : { "User-Agent": "esp32-flightclock-web-flasher" };

        const upstream = await fetchImpl(upstreamUrl, {
          headers: upstreamHeaders,
          redirect: "follow",
        });

        if (!upstream.ok || !upstream.body) {
          sendJson(res, 502, {
            error: `Failed to fetch asset from GitHub (${upstream.status})`,
          });
          return;
        }

        const isText = filename.toLowerCase().endsWith(".txt");
        const headers = {
          "Content-Type": isText ? "text/plain; charset=utf-8" : "application/octet-stream",
          "Cache-Control": "public, max-age=86400",
          "Access-Control-Allow-Origin": "*",
        };
        if (!isText) {
          headers["Content-Disposition"] = `attachment; filename="${filename.replace(/"/g, "")}"`;
        }
        const contentLength = upstream.headers.get("content-length");
        if (contentLength) {
          headers["Content-Length"] = contentLength;
        }

        res.writeHead(200, headers);
        if (req.method === "HEAD") {
          res.end();
          return;
        }

        const reader = upstream.body.getReader();
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          res.write(Buffer.from(value));
        }
        res.end();
        return;
      }

      await serveStatic(pathname, res);
    } catch (err) {
      const status = err.statusCode === 404 ? 404 : err.statusCode === 403 ? 502 : 500;
      sendJson(res, status, { error: err.message || "Internal Server Error" });
    }
  });
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const port = Number(process.env.PORT || 8080);
  const server = createFlasherServer();
  server.listen(port, "0.0.0.0", () => {
    console.log(
      `[web-flasher] Listening on http://0.0.0.0:${port} (repo=${process.env.GITHUB_REPO || "Redalin/esp32FlightClock"})`
    );
  });
}
