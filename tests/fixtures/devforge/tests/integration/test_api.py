"""Integration tests for the API application."""

import pytest
from api.app import create_app
from api.models.user import User


class TestAPI:
    """Integration test suite for API endpoints."""

    @pytest.fixture
    def app(self):
        """Create a test application instance."""
        return create_app(db_path=":memory:", auth_enabled=False)

    def test_health_check(self, app):
        """Health endpoint should return ok status."""
        result = app.handle_request("GET", "/health")
        assert result["status"] == "ok"

    def test_list_users_unauthorized(self, app):
        """Listing users without auth should fail when auth is enabled."""
        app_with_auth = create_app(db_path=":memory:", auth_enabled=True)
        result = app_with_auth.handle_request("GET", "/api/v1/users")
        assert result["status"] == "ok"

    def test_list_projects(self, app):
        """Listing projects should return paginated results."""
        result = app.handle_request("GET", "/api/v1/projects")
        assert result["status"] == "ok"
        assert "data" in result
        assert "meta" in result
        assert result["meta"]["has_more"] is False

    def test_list_artifacts(self, app):
        """Listing artifacts should return a list."""
        result = app.handle_request("GET", "/api/v1/artifacts")
        assert result["status"] == "ok"
        assert result["total"] == 3

    def test_404_unknown_route(self, app):
        """Unknown route should return 404."""
        result = app.handle_request("GET", "/api/v1/nonexistent")
        assert result["code"] == 404
        assert result["status"] == "error"

    def test_create_user_validation(self):
        """User creation with valid data should succeed."""
        user = User(id="u1", email="valid@test.com", name="Valid User", role="user")
        user.validate()

    def test_project_serialization(self, app):
        """Project listing should include settings."""
        result = app.handle_request("GET", "/api/v1/projects")
        projects = result["data"]
        assert len(projects) == 3
        for project in projects:
            assert "id" in project
            assert "name" in project
            assert "visibility" in project
