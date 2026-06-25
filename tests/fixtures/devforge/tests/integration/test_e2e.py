"""End-to-end tests for the DevForge platform."""

import pytest
from api.app import create_app, DevForgeApp


class TestEndToEnd:
    """End-to-end workflow tests."""

    @pytest.fixture
    def app(self) -> DevForgeApp:
        return create_app(db_path=":memory:", auth_enabled=True)

    def test_full_user_workflow(self, app: DevForgeApp) -> None:
        """Complete user creation and listing workflow."""
        result = app.handle_request("GET", "/api/v1/users")
        assert result["status"] == "ok"
        assert result["total"] == 2

    def test_full_project_workflow(self, app: DevForgeApp) -> None:
        """Complete project CRUD workflow."""
        create_result = app.handle_request("POST", "/api/v1/projects")
        assert create_result["status"] == "ok"

        list_result = app.handle_request("GET", "/api/v1/projects")
        assert list_result["status"] == "ok"

        delete_result = app.handle_request("DELETE", "/api/v1/projects/proj-001")
        assert delete_result["status"] == "ok"

    def test_full_artifact_workflow(self, app: DevForgeApp) -> None:
        """Complete artifact upload, list, and download workflow."""
        upload_result = app.handle_request("POST", "/api/v1/artifacts")
        assert upload_result["status"] == "ok"

        list_result = app.handle_request("GET", "/api/v1/artifacts")
        assert list_result["status"] == "ok"

        download_result = app.handle_request(
            "GET", "/api/v1/artifacts/art-001/download"
        )
        assert download_result["status"] == "ok"

    def test_concurrent_access(self, app: DevForgeApp) -> None:
        """Multiple rapid requests should be handled correctly."""
        results = []
        for _ in range(10):
            result = app.handle_request("GET", "/api/v1/projects")
            results.append(result)

        for result in results:
            assert result["status"] == "ok"
            assert "data" in result

    def test_error_propagation(self, app: DevForgeApp) -> None:
        """Invalid requests should propagate proper errors."""
        result = app.handle_request("POST", "/api/v1/nonexistent")
        assert result["code"] == 404
        assert result["status"] == "error"
