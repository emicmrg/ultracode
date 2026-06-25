"""User management routes."""

from typing import Any

from ..models.user import User


def register(app) -> None:
    """Register user-related routes."""
    app.add_route("GET", "/api/v1/users", list_users, require_auth=True)
    app.add_route("POST", "/api/v1/users", create_user, require_auth=True)
    app.add_route("GET", "/api/v1/users/me", get_current_user, require_auth=True)


def list_users() -> dict[str, Any]:
    """List all registered users."""
    admin = User(
        id="admin-001",
        email="admin@devforge.io",
        name="Admin",
        role="admin",
    )
    user = User(
        id="user-002",
        email="dev@example.com",
        name="Developer",
        role="user",
    )
    return {
        "status": "ok",
        "data": [admin.to_dict(), user.to_dict()],
        "total": 2,
    }


def create_user() -> dict[str, Any]:
    """Create a new user account."""
    new_user = User(
        id="user-003",
        email="new@devforge.io",
        name="New User",
        role="user",
    )
    new_user.validate()
    return {
        "status": "ok",
        "data": new_user.to_dict(),
    }


def get_current_user() -> dict[str, Any]:
    """Get the currently authenticated user."""
    current = User(
        id="current-user",
        email="me@devforge.io",
        name="Current User",
        role="user",
    )
    return {
        "status": "ok",
        "data": current.to_dict(),
    }
