import test from "node:test";
import assert from "node:assert/strict";
import {
  buildManifest,
  createFlasherServer,
  groupReleasesByBranch,
  normalizeBranch,
  selectFirmwareAssets,
} from "./server.mjs";

const SAMPLE_RELEASES = [
  {
    tag_name: "v1.0.9",
    name: "Flight Clock v1.0.9",
    target_commitish: "main",
    prerelease: false,
    draft: false,
    published_at: "2026-11-28T01:00:00Z",
    html_url: "https://github.com/Redalin/esp32FlightClock/releases/tag/v1.0.9",
    body: "## Highlights\n- Added compass heading\n- Fixed Wi-Fi reconnect",
    assets: [
      {
        name: "flightclock-v1.0.9-factory.bin",
        size: 1048576,
        browser_download_url: "https://example.com/factory.bin",
        url: "https://api.github.com/repos/Redalin/esp32FlightClock/releases/assets/1",
      },
      {
        name: "flightclock-v1.0.9-ota.bin",
        size: 950000,
        browser_download_url: "https://example.com/ota.bin",
        url: "https://api.github.com/repos/Redalin/esp32FlightClock/releases/assets/2",
      },
      {
        name: "SHA256SUMS.txt",
        size: 160,
        browser_download_url: "https://example.com/SHA256SUMS.txt",
        url: "https://api.github.com/repos/Redalin/esp32FlightClock/releases/assets/3",
      },
    ],
  },
  {
    tag_name: "v1.1.0-beta.12.abc1234",
    name: "Flight Clock v1.1.0-beta.12.abc1234",
    target_commitish: "beta",
    prerelease: true,
    draft: false,
    published_at: "2026-11-28T02:00:00Z",
    html_url: "https://github.com/Redalin/esp32FlightClock/releases/tag/v1.1.0-beta.12.abc1234",
    body: "Beta build for testing.",
    assets: [
      {
        name: "flightclock-v1.1.0-beta.12.abc1234-factory.bin",
        size: 1050000,
        browser_download_url: "https://example.com/beta-factory.bin",
        url: "https://api.github.com/repos/Redalin/esp32FlightClock/releases/assets/4",
      },
    ],
  },
  {
    tag_name: "v0.0.1-draft",
    draft: true,
    target_commitish: "main",
    assets: [],
  },
];

test("normalizeBranch maps commit SHAs to default branch", () => {
  assert.equal(normalizeBranch("main", "main"), "main");
  assert.equal(normalizeBranch("beta", "main"), "beta");
  assert.equal(
    normalizeBranch("0123456789abcdef0123456789abcdef01234567", "main"),
    "main"
  );
});

test("selectFirmwareAssets identifies factory, ota, and checksum assets", () => {
  const res = selectFirmwareAssets(SAMPLE_RELEASES[0].assets);
  assert.equal(res.factoryAsset?.name, "flightclock-v1.0.9-factory.bin");
  assert.equal(res.otaAsset?.name, "flightclock-v1.0.9-ota.bin");
  assert.equal(res.checksumAsset?.name, "SHA256SUMS.txt");
});

test("groupReleasesByBranch filters drafts and orders default branch first", () => {
  const grouped = groupReleasesByBranch(SAMPLE_RELEASES, "main");
  assert.deepEqual(grouped.branches, ["main", "beta"]);
  assert.equal(grouped.releasesByBranch.main.length, 1);
  assert.equal(grouped.releasesByBranch.beta.length, 1);
  assert.equal(grouped.releasesByTag.has("v0.0.1-draft"), false);
});

test("buildManifest creates a valid ESP Web Tools ESP32-C3 manifest", () => {
  const grouped = groupReleasesByBranch(SAMPLE_RELEASES, "main");
  const manifest = buildManifest(grouped.releasesByTag.get("v1.0.9"), "Flight Clock");
  assert.equal(manifest.name, "Flight Clock");
  assert.equal(manifest.version, "v1.0.9");
  assert.equal(manifest.builds[0].chipFamily, "ESP32-C3");
  assert.equal(
    manifest.builds[0].parts[0].path,
    "/api/firmware/v1.0.9/flightclock-v1.0.9-factory.bin"
  );
  assert.equal(manifest.builds[0].parts[0].offset, 0);
});

test("HTTP server serves health, releases, and manifest endpoints", async () => {
  const mockFetch = async () =>
    new Response(JSON.stringify(SAMPLE_RELEASES), {
      status: 200,
      headers: { "Content-Type": "application/json" },
    });

  const server = createFlasherServer({
    repo: "Redalin/esp32FlightClock",
    fetchImpl: mockFetch,
  });

  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const { port } = server.address();
  const base = `http://127.0.0.1:${port}`;

  try {
    const health = await fetch(`${base}/health`).then((r) => r.json());
    assert.equal(health.status, "ok");

    const releases = await fetch(`${base}/api/releases`).then((r) => r.json());
    assert.deepEqual(releases.branches, ["main", "beta"]);
    assert.equal(releases.releasesByBranch.main[0].tag, "v1.0.9");
    assert.equal(releases.releasesByBranch.main[0]._rawAssets, undefined);

    const manifest = await fetch(`${base}/api/manifest/v1.0.9`).then((r) => r.json());
    assert.equal(manifest.builds[0].chipFamily, "ESP32-C3");
    assert.equal(manifest.builds[0].parts[0].offset, 0);
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
});
