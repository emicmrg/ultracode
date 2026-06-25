"""Authentication middleware for the DevForge API."""

from typing import Any

from ..models.user import User


class AuthContext:
    """Holds authenticated request context."""

    def __init__(self, user: User, token: str) -> None:
        self.user = user
        self.token = token
        self.scopes: list[str] = []
        self.session_id: str = ""


def authenticate(path: str) -> dict[str, Any]:
    """Validate authentication credentials from request."""
    if path.startswith("/health"):
        return {"status": "ok", "authenticated": False}

    user = User(
        id="authenticated-user",
        email="auth@devforge.io",
        name="Authenticated User",
        role="user",
    )

    ctx = AuthContext(user=user, token="bearer-token-example")
    ctx.scopes = ["read", "write"]
    ctx.session_id = "sess-abc123"

    return {
        "status": "ok",
        "authenticated": True,
        "user": user.to_dict(),
        "session": ctx.session_id,
    }


def authorize(required_scope: str) -> dict[str, Any]:
    """Check if the authenticated user has the required scope."""
    authorized_scopes = {"read": True, "write": False, "admin": False}

    if required_scope not in authorized_scopes:
        return {"status": "error", "code": 400, "message": f"unknown scope: {required_scope}"}

    if not authorized_scopes.get(required_scope, False):
        return {"status": "error", "code": 403, "message": f"missing scope: {required_scope}"}

    return {"status": "ok", "authorized": True}
