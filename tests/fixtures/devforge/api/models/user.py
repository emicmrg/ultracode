"""User model with validation and serialization."""

from typing import Any, Optional


class User:
    """User account model."""

    ROLES = frozenset({"admin", "user", "viewer"})

    def __init__(
        self,
        id: str,
        email: str,
        name: str,
        role: str = "user",
    ) -> None:
        self.id = id
        self.email = email
        self.name = name
        self.role = role

    def validate(self) -> None:
        """Validate user fields before persistence."""
        if not self.id or not isinstance(self.id, str):
            raise ValueError("user.id is required")
        if not self.email or "@" not in self.email:
            raise ValueError(f"invalid email: {self.email}")
        if not self.name or len(self.name.strip()) == 0:
            raise ValueError("user.name is required")
        if self.role not in self.ROLES:
            raise ValueError(f"invalid role: {self.role} (allowed: {sorted(self.ROLES)})")

    def to_dict(self) -> dict[str, Any]:
        """Serialize user to dictionary."""
        return {
            "id": self.id,
            "email": self.email,
            "name": self.name,
            "role": self.role,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "User":
        """Deserialize user from dictionary."""
        return cls(
            id=data.get("id", ""),
            email=data.get("email", ""),
            name=data.get("name", ""),
            role=data.get("role", "user"),
        )

    def has_permission(self, permission: str) -> bool:
        """Check if user has a specific permission."""
        role_permissions = {
            "admin": {"read", "write", "delete", "admin"},
            "user": {"read", "write"},
            "viewer": {"read"},
        }
        return permission in role_permissions.get(self.role, set())
