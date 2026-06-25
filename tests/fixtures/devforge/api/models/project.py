"""Project model with nested settings and validation."""

from dataclasses import dataclass, field
from typing import Any


@dataclass
class ProjectSettings:
    """Nested settings for project configuration."""

    max_artifacts: int = 100
    retention_days: int = 90
    default_branch: str = "main"
    enable_ci: bool = False

    def validate(self) -> None:
        """Validate settings constraints."""
        if self.max_artifacts < 1 or self.max_artifacts > 10000:
            raise ValueError(f"max_artifacts out of range: {self.max_artifacts}")
        if self.retention_days < 1 or self.retention_days > 3650:
            raise ValueError(f"retention_days out of range: {self.retention_days}")
        if not self.default_branch:
            raise ValueError("default_branch is required")
        if self.default_branch.startswith("/") or ".." in self.default_branch:
            raise ValueError(f"invalid default_branch: {self.default_branch}")

    def to_dict(self) -> dict[str, Any]:
        """Serialize settings to dictionary."""
        return {
            "max_artifacts": self.max_artifacts,
            "retention_days": self.retention_days,
            "default_branch": self.default_branch,
            "enable_ci": self.enable_ci,
        }


@dataclass
class Project:
    """Project model for artifact organization."""

    id: str
    name: str
    owner_id: str
    visibility: str = "private"
    settings: ProjectSettings = field(default_factory=ProjectSettings)

    VALID_VISIBILITIES = frozenset({"public", "private", "internal"})

    def validate(self) -> None:
        """Validate project fields before persistence."""
        if not self.id or not isinstance(self.id, str):
            raise ValueError("project.id is required")
        if not self.name or len(self.name.strip()) == 0:
            raise ValueError("project.name is required")
        if len(self.name) > 128:
            raise ValueError("project.name exceeds 128 characters")
        if not self.owner_id:
            raise ValueError("project.owner_id is required")
        if self.visibility not in self.VALID_VISIBILITIES:
            raise ValueError(
                f"invalid visibility: {self.visibility} "
                f"(allowed: {sorted(self.VALID_VISIBILITIES)})"
            )
        self.settings.validate()

    def to_dict(self) -> dict[str, Any]:
        """Serialize project to dictionary."""
        return {
            "id": self.id,
            "name": self.name,
            "owner_id": self.owner_id,
            "visibility": self.visibility,
            "settings": self.settings.to_dict(),
        }

    def is_public(self) -> bool:
        """Check if project is publicly visible."""
        return self.visibility == "public"

    def can_delete(self) -> bool:
        """Check if project can be deleted."""
        return self.visibility != "internal"
