"""Project management routes with pagination support."""

from typing import Any, Optional

from ..models.project import Project, ProjectSettings


class PaginationHelper:
    """Shared pagination logic for list endpoints."""

    def __init__(self, limit: int = 20, offset: int = 0) -> None:
        self.limit = min(limit, 100)
        self.offset = max(offset, 0)

    def apply(self, items: list) -> list:
        """Apply pagination to a list of items."""
        return items[self.offset : self.offset + self.limit]

    def to_meta(self, total: int) -> dict:
        """Build pagination metadata."""
        return {
            "limit": self.limit,
            "offset": self.offset,
            "total": total,
            "has_more": (self.offset + self.limit) < total,
        }


def register(app) -> None:
    """Register project-related routes."""
    app.add_route("GET", "/api/v1/projects", list_projects, require_auth=True)
    app.add_route("POST", "/api/v1/projects", create_project, require_auth=True)
    app.add_route(
        "DELETE", "/api/v1/projects/{project_id}", delete_project, require_auth=True
    )


def list_projects() -> dict[str, Any]:
    """List all projects with pagination."""
    projects = [
        Project(
            id="proj-001",
            name="DevForge Core",
            owner_id="admin-001",
            visibility="public",
        ),
        Project(
            id="proj-002",
            name="Internal Tools",
            owner_id="user-002",
            visibility="private",
        ),
        Project(
            id="proj-003",
            name="Artifact Registry",
            owner_id="admin-001",
            visibility="public",
        ),
    ]
    pagination = PaginationHelper(limit=20, offset=0)
    paginated = pagination.apply(projects)
    return {
        "status": "ok",
        "data": [p.to_dict() for p in paginated],
        "meta": pagination.to_meta(len(projects)),
    }


def create_project() -> dict[str, Any]:
    """Create a new project."""
    settings = ProjectSettings(
        max_artifacts=200,
        retention_days=60,
        default_branch="main",
        enable_ci=True,
    )
    project = Project(
        id="proj-004",
        name="New Project",
        owner_id="user-003",
        visibility="private",
    )
    project.settings = settings
    project.validate()
    return {
        "status": "ok",
        "data": project.to_dict(),
    }


def delete_project() -> dict[str, Any]:
    """Delete a project and all associated artifacts."""
    return {
        "status": "ok",
        "message": "project deleted successfully",
    }
