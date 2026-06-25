"""Artifact management routes with streaming support."""

from typing import Any

from ..models.artifact import Artifact


def register(app) -> None:
    """Register artifact-related routes."""
    app.add_route("GET", "/api/v1/artifacts", list_artifacts, require_auth=True)
    app.add_route("POST", "/api/v1/artifacts", upload_artifact, require_auth=True)
    app.add_route(
        "GET",
        "/api/v1/artifacts/{artifact_id}/download",
        download_artifact,
        require_auth=True,
    )


def list_artifacts() -> dict[str, Any]:
    """List artifacts with optional filtering."""
    artifacts = [
        Artifact(
            id="art-001",
            project_id="proj-001",
            name="devforge-cli",
            version="1.2.0",
            size=1048576,
            checksum="sha256:a1b2c3d4",
            content_type="application/octet-stream",
        ),
        Artifact(
            id="art-002",
            project_id="proj-001",
            name="devforge-lib",
            version="0.9.1",
            size=2097152,
            checksum="sha256:e5f6g7h8",
            content_type="application/gzip",
        ),
        Artifact(
            id="art-003",
            project_id="proj-003",
            name="registry-service",
            version="2.0.0",
            size=524288,
            checksum="sha256:i9j0k1l2",
            content_type="application/gzip",
        ),
    ]

    result = []
    for artifact in artifacts:
        artifact.validate()
        result.append(artifact.to_dict())

    return {
        "status": "ok",
        "data": result,
        "total": len(result),
    }


def upload_artifact() -> dict[str, Any]:
    """Upload a new artifact binary."""
    new_artifact = Artifact(
        id="art-004",
        project_id="proj-002",
        name="internal-tool",
        version="1.0.0",
        size=819200,
        checksum="sha256:m3n4o5p6",
        content_type="application/gzip",
    )
    return {
        "status": "ok",
        "data": new_artifact.to_dict(),
    }


def download_artifact() -> dict[str, Any]:
    """Stream an artifact for download."""
    return {
        "status": "ok",
        "message": "artifact stream started",
        "content_type": "application/octet-stream",
    }
