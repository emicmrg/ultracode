"""Unit tests for User model."""

import pytest
from api.models.user import User


class TestUserModel:
    """Test suite for User model validation and serialization."""

    def test_valid_user(self) -> None:
        """User with valid fields should pass validation."""
        user = User(id="u1", email="test@example.com", name="Test User", role="user")
        user.validate()

    def test_invalid_email(self) -> None:
        """User with invalid email should raise ValueError."""
        user = User(id="u1", email="not-an-email", name="Test", role="user")
        with pytest.raises(ValueError, match="invalid email"):
            user.validate()

    def test_empty_name(self) -> None:
        """User with empty name should raise ValueError."""
        user = User(id="u1", email="test@example.com", name="", role="user")
        with pytest.raises(ValueError, match="user.name is required"):
            user.validate()

    def test_invalid_role(self) -> None:
        """User with invalid role should raise ValueError."""
        user = User(id="u1", email="test@example.com", name="Test", role="superadmin")
        with pytest.raises(ValueError, match="invalid role"):
            user.validate()

    def test_to_dict(self) -> None:
        """Serialization should produce correct dictionary."""
        user = User(id="u1", email="test@example.com", name="Test", role="user")
        result = user.to_dict()
        assert result == {
            "id": "u1",
            "email": "test@example.com",
            "name": "Test",
            "role": "user",
        }

    def test_from_dict(self) -> None:
        """Deserialization should produce correct User."""
        data = {"id": "u2", "email": "dev@example.com", "name": "Dev", "role": "admin"}
        user = User.from_dict(data)
        assert user.id == "u2"
        assert user.email == "dev@example.com"
        assert user.role == "admin"

    def test_has_permission_admin(self) -> None:
        """Admin should have all permissions."""
        user = User(id="a", email="a@b.com", name="Admin", role="admin")
        assert user.has_permission("read")
        assert user.has_permission("write")
        assert user.has_permission("delete")
        assert user.has_permission("admin")
        assert not user.has_permission("nonexistent")

    def test_has_permission_viewer(self) -> None:
        """Viewer should only have read permission."""
        user = User(id="v", email="v@b.com", name="Viewer", role="viewer")
        assert user.has_permission("read")
        assert not user.has_permission("write")
        assert not user.has_permission("delete")

    def test_validate_all_roles(self) -> None:
        """All valid roles should pass validation."""
        for role in User.ROLES:
            user = User(id="u", email="u@b.com", name="U", role=role)
            user.validate()
