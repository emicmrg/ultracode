/** Artifact viewer with metadata display and download actions. */

import type { Artifact } from "../services/api";
import { formatBytes, formatDate } from "../utils/format";
import { validateChecksum } from "../utils/validation";

interface ArtifactViewerProps {
  artifact: Artifact;
  projectId: string;
  onDownload: (artifactId: string) => void;
  onScan: (artifactId: string) => void;
}

interface MetadataDisplay {
  label: string;
  value: string;
  mono?: boolean;
}

function buildMetadata(artifact: Artifact): MetadataDisplay[] {
  return [
    { label: "Name", value: artifact.name },
    { label: "Version", value: artifact.version },
    { label: "Size", value: formatBytes(artifact.size) },
    { label: "Type", value: artifact.contentType },
    { label: "Checksum", value: artifact.checksum, mono: true },
    { label: "Uploaded", value: formatDate(artifact.createdAt) },
    { label: "Project", value: artifact.projectId, mono: true },
  ];
}

function renderMetadataTable(metadata: MetadataDisplay[]): string {
  return metadata
    .map((m) => {
      const value = m.mono ? `<code>${m.value}</code>` : m.value;
      return `<tr><th>${m.label}</th><td>${value}</td></tr>`;
    })
    .join("");
}

export function createArtifactViewer(
  container: HTMLElement,
  props: ArtifactViewerProps
): void {
  const { artifact, projectId, onDownload, onScan } = props;

  const checksumValid = validateChecksum(artifact.checksum);

  const metadata = buildMetadata(artifact);

  container.innerHTML = `
    <div class="artifact-viewer">
      <div class="artifact-header">
        <h2>${artifact.name} v${artifact.version}</h2>
        <span class="checksum-status ${
          checksumValid ? "valid" : "invalid"
        }">${checksumValid ? "valid" : "invalid"}</span>
      </div>
      <table class="metadata-table">
        ${renderMetadataTable(metadata)}
      </table>
      <div class="artifact-actions">
        <button id="download-btn">Download</button>
        <button id="scan-btn">Scan</button>
      </div>
    </div>
  `;

  const downloadBtn = container.querySelector("#download-btn");
  const scanBtn = container.querySelector("#scan-btn");

  if (downloadBtn) {
    downloadBtn.addEventListener("click", () => onDownload(artifact.id));
  }
  if (scanBtn) {
    scanBtn.addEventListener("click", () => onScan(artifact.id));
  }
}

export { buildMetadata };
export type { ArtifactViewerProps, MetadataDisplay };
