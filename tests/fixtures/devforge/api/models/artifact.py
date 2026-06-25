"""Artifact model with file metadata and validation."""

from dataclasses import dataclass
from typing import Any, Optional


@dataclass
class Artifact:
    """Artifact model representing a stored binary or package."""

    id: str
    project_id: str
    name: str
    version: str
    size: int = 0
    checksum: str = ""
    content_type: str = "application/octet-stream"
    uploaded_by: str = ""

    def validate(self) -> None:
        """Validate artifact fields before upload."""
        if not self.id or not isinstance(self.id, str):
            raise ValueError("artifact.id is required")
        if not self.project_id:
            raise ValueError("artifact.project_id is required")
        if not self.name or len(self.name.strip()) == 0:
            raise ValueError("artifact.name is required")
        if len(self.name) > 256:
            raise ValueError("artifact.name exceeds 256 characters")
        if not self.version:
            raise ValueError("artifact.version is required")
        if self.size < 0:
            raise ValueError(f"artifact.size cannot be negative: {self.size}")
        if self.size > 10737418240:  # 10 GiB
            raise ValueError(f"artifact.size exceeds 10 GiB limit: {self.size}")

    def to_dict(self) -> dict[str, Any]:
        """Serialize artifact to dictionary."""
        return {
            "id": self.id,
            "project_id": self.project_id,
            "name": self.name,
            "version": self.version,
            "size": self.size,
            "checksum": self.checksum,
            "content_type": self.content_type,
            "uploaded_by": self.uploaded_by,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Artifact":
        """Deserialize artifact from dictionary."""
        return cls(
            id=data.get("id", ""),
            project_id=data.get("project_id", ""),
            name=data.get("name", ""),
            version=data.get("version", ""),
            size=data.get("size", 0),
            checksum=data.get("checksum", ""),
            content_type=data.get("content_type", "application/octet-stream"),
            uploaded_by=data.get("uploaded_by", ""),
        )

    def human_size(self) -> str:
        """Format artifact size in human-readable form."""
        for unit in ["B", "KiB", "MiB", "GiB"]:
            if self.size < 1024:
                return f"{self.size} {unit}"
            self.size //= 1024
        return f"{self.size} TiB"
