const els = {
  appTitle: document.getElementById("app-title"),
  repoLink: document.getElementById("repo-link"),
  refreshBtn: document.getElementById("refresh-btn"),
  compatBanner: document.getElementById("compat-banner"),
  errorBanner: document.getElementById("error-banner"),
  syncStatus: document.getElementById("sync-status"),
  branchSelect: document.getElementById("branch-select"),
  versionSelect: document.getElementById("version-select"),
  releaseMeta: document.getElementById("release-meta"),
  metaBadge: document.getElementById("meta-badge"),
  metaDate: document.getElementById("meta-date"),
  metaSize: document.getElementById("meta-size"),
  metaGithubLink: document.getElementById("meta-github-link"),
  installBtn: document.getElementById("install-btn"),
  installActivate: document.getElementById("install-activate"),
  downloadLinks: document.getElementById("download-links"),
  otaDownload: document.getElementById("ota-download"),
  factoryDownload: document.getElementById("factory-download"),
  notesVersion: document.getElementById("notes-version"),
  releaseNotes: document.getElementById("release-notes"),
};

let state = {
  repo: "",
  defaultBranch: "main",
  branches: [],
  releasesByBranch: {},
};

let domPurifyPromise;

function loadDOMPurify() {
  domPurifyPromise ??= import("https://esm.sh/dompurify@3.2.4").then(
    (mod) => mod.default ?? mod
  );
  return domPurifyPromise;
}

async function safeSetHTML(el, html) {
  const elements = ["p", "h3", "h4", "ul", "ol", "li", "strong", "em", "code", "pre", "br", "a"];
  const attributes = ["href", "target", "rel"];

  if ("setHTML" in Element.prototype) {
    el.setHTML(html, { sanitizer: { elements, attributes } });
  } else {
    const DOMPurify = await loadDOMPurify();
    el.innerHTML = DOMPurify.sanitize(html, {
      ALLOWED_TAGS: [...elements, "#text"],
      ALLOWED_ATTR: attributes,
      ALLOW_ARIA_ATTR: false,
      ALLOW_DATA_ATTR: false,
      KEEP_CONTENT: false,
    });
  }
}

function escapeHtml(str) {
  return str
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function formatInlineMarkdown(text) {
  let out = escapeHtml(text);
  out = out.replace(/`([^`]+)`/g, "<code>$1</code>");
  out = out.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
  out = out.replace(/\*([^*]+)\*/g, "<em>$1</em>");
  out = out.replace(
    /\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/g,
    '<a href="$2" target="_blank" rel="noopener noreferrer">$1</a>'
  );
  out = out.replace(
    /(^|[\s(])(https?:\/\/[^\s<)]+)/g,
    '$1<a href="$2" target="_blank" rel="noopener noreferrer">$2</a>'
  );
  return out;
}

function markdownToHtml(markdown) {
  if (!markdown || !markdown.trim()) {
    return "<p>No release notes were provided for this version.</p>";
  }

  const lines = markdown.replace(/\r\n/g, "\n").split("\n");
  const html = [];
  let inList = false;

  const closeList = () => {
    if (inList) {
      html.push("</ul>");
      inList = false;
    }
  };

  for (const rawLine of lines) {
    const line = rawLine.trim();
    if (!line) {
      closeList();
      continue;
    }
    if (line.startsWith("### ")) {
      closeList();
      html.push(`<h4>${formatInlineMarkdown(line.slice(4))}</h4>`);
    } else if (line.startsWith("## ") || line.startsWith("# ")) {
      closeList();
      const title = line.replace(/^#{1,2}\s+/, "");
      html.push(`<h3>${formatInlineMarkdown(title)}</h3>`);
    } else if (/^[-*]\s+/.test(line)) {
      if (!inList) {
        html.push("<ul>");
        inList = true;
      }
      html.push(`<li>${formatInlineMarkdown(line.replace(/^[-*]\s+/, ""))}</li>`);
    } else {
      closeList();
      html.push(`<p>${formatInlineMarkdown(line)}</p>`);
    }
  }

  closeList();
  return html.join("\n");
}

function formatBytes(bytes) {
  if (!bytes) return "";
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}

function formatDate(iso) {
  if (!iso) return "";
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return iso;
  return new Intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
    timeStyle: "short",
  }).format(d);
}

function checkBrowserCompatibility() {
  if (!window.isSecureContext) {
    els.compatBanner.textContent =
      "Web Serial requires HTTPS (or localhost). Please access this flasher over HTTPS to flash via USB.";
    els.compatBanner.hidden = false;
  } else if (!("serial" in navigator)) {
    els.compatBanner.textContent =
      "Your browser does not support Web Serial. Please open this page in Google Chrome, Microsoft Edge, or Opera to flash over USB.";
    els.compatBanner.hidden = false;
  } else {
    els.compatBanner.hidden = true;
  }
}

function showError(message) {
  if (!message) {
    els.errorBanner.hidden = true;
    els.errorBanner.textContent = "";
    return;
  }
  els.errorBanner.textContent = message;
  els.errorBanner.hidden = false;
}

function populateBranches() {
  els.branchSelect.replaceChildren();
  if (!state.branches.length) {
    const opt = document.createElement("option");
    opt.value = "";
    opt.textContent = "No releases found";
    els.branchSelect.append(opt);
    els.branchSelect.disabled = true;
    els.versionSelect.disabled = true;
    return;
  }

  for (const branch of state.branches) {
    const count = state.releasesByBranch[branch]?.length ?? 0;
    const opt = document.createElement("option");
    opt.value = branch;
    opt.textContent = `${branch} (${count})`;
    els.branchSelect.append(opt);
  }

  els.branchSelect.disabled = false;
  const preferred = state.branches.includes(state.defaultBranch)
    ? state.defaultBranch
    : state.branches[0];
  els.branchSelect.value = preferred;
  populateVersions(preferred);
}

function populateVersions(branch) {
  els.versionSelect.replaceChildren();
  const list = state.releasesByBranch[branch] ?? [];

  if (!list.length) {
    const opt = document.createElement("option");
    opt.value = "";
    opt.textContent = "No versions on this branch";
    els.versionSelect.append(opt);
    els.versionSelect.disabled = true;
    renderSelectedRelease(null);
    return;
  }

  list.forEach((rel, index) => {
    const opt = document.createElement("option");
    opt.value = rel.tag;
    const suffix = index === 0 ? " (Latest)" : rel.prerelease ? " (Pre-release)" : "";
    opt.textContent = `${rel.tag}${suffix}`;
    els.versionSelect.append(opt);
  });

  els.versionSelect.disabled = false;
  els.versionSelect.value = list[0].tag;
  renderSelectedRelease(list[0]);
}

async function renderSelectedRelease(release) {
  if (!release) {
    els.releaseMeta.hidden = true;
    els.downloadLinks.hidden = true;
    els.installActivate.disabled = true;
    els.installBtn.removeAttribute("manifest");
    els.notesVersion.textContent = "";
    await safeSetHTML(els.releaseNotes, "<p>No firmware release selected.</p>");
    return;
  }

  els.releaseMeta.hidden = false;
  els.metaBadge.textContent = release.prerelease ? "Pre-release" : "Stable";
  els.metaDate.textContent = release.publishedAt ? `Published ${formatDate(release.publishedAt)}` : "";
  els.metaSize.textContent = release.factoryAsset
    ? `Factory image: ${formatBytes(release.factoryAsset.size)}`
    : "No factory image attached";
  els.metaGithubLink.href = release.htmlUrl || `https://github.com/${state.repo}/releases`;

  const encodedTag = encodeURIComponent(release.tag);

  if (release.flashable) {
    els.installBtn.setAttribute("manifest", `/api/manifest/${encodedTag}`);
    els.installActivate.disabled = false;
    els.installActivate.textContent = `Install ${release.tag} via USB`;
  } else {
    els.installBtn.removeAttribute("manifest");
    els.installActivate.disabled = true;
    els.installActivate.textContent = "Factory binary unavailable for this release";
  }

  const hasDownloads = Boolean(release.otaAsset || release.factoryAsset);
  els.downloadLinks.hidden = !hasDownloads;

  if (release.otaAsset) {
    els.otaDownload.href = `/api/firmware/${encodedTag}/${encodeURIComponent(release.otaAsset.name)}`;
    els.otaDownload.hidden = false;
  } else {
    els.otaDownload.hidden = true;
  }

  if (release.factoryAsset) {
    els.factoryDownload.href = `/api/firmware/${encodedTag}/${encodeURIComponent(release.factoryAsset.name)}`;
    els.factoryDownload.hidden = false;
  } else {
    els.factoryDownload.hidden = true;
  }

  els.notesVersion.textContent = `${release.branch} · ${release.tag}`;
  await safeSetHTML(els.releaseNotes, markdownToHtml(release.notes));
}

async function loadReleases(forceRefresh = false) {
  showError("");
  els.refreshBtn.disabled = true;
  els.syncStatus.textContent = forceRefresh ? "Refreshing from GitHub…" : "Loading releases…";

  try {
    const res = await fetch(forceRefresh ? "/api/releases?refresh=1" : "/api/releases");
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || `Request failed (${res.status})`);
    }

    state = data;
    if (data.appTitle) {
      els.appTitle.textContent = data.appTitle;
      document.title = `${data.appTitle} — Web Flasher`;
    }
    if (data.repo) {
      els.repoLink.href = `https://github.com/${data.repo}`;
    }

    const total = Object.values(data.releasesByBranch || {}).reduce((sum, l) => sum + l.length, 0);
    els.syncStatus.textContent =
      total > 0
        ? `${total} release${total === 1 ? "" : "s"} synced · ${formatDate(data.fetchedAt)}`
        : "No GitHub releases published yet";

    populateBranches();
  } catch (err) {
    showError(`Could not load releases from GitHub: ${err.message}`);
    els.syncStatus.textContent = "Sync failed";
  } finally {
    els.refreshBtn.disabled = false;
  }
}

els.branchSelect.addEventListener("change", () => {
  populateVersions(els.branchSelect.value);
});

els.versionSelect.addEventListener("change", () => {
  const list = state.releasesByBranch[els.branchSelect.value] ?? [];
  const selected = list.find((r) => r.tag === els.versionSelect.value) || null;
  renderSelectedRelease(selected);
});

els.refreshBtn.addEventListener("click", () => loadReleases(true));

checkBrowserCompatibility();
loadReleases(false);
