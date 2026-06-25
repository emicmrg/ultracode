"""DevForge API application factory."""

from typing import Optional

from .routes import users, projects, artifacts
from .middleware import auth, ratelimit


class AppState:
    """Shared application state."""

    def __init__(self) -> None:
        self.db_path: str = "devforge.db"
        self.auth_enabled: bool = True
        self.rate_limit: int = 100


class DevForgeApp:
    """Main application that orchestrates routes and middleware."""

    def __init__(
        self,
        db_path: Optional[str] = None,
        auth_enabled: bool = True,
        rate_limit: int = 100,
    ) -> None:
        self.state = AppState()
        if db_path:
            self.state.db_path = db_path
        self.state.auth_enabled = auth_enabled
        self.state.rate_limit = rate_limit
        self._routes: dict[str, list] = {}

    def register_routes(self) -> None:
        """Register all API routes."""
        users.register(self)
        projects.register(self)
        artifacts.register(self)

    def add_route(
        self,
        method: str,
        path: str,
        handler,
        require_auth: bool = True,
    ) -> None:
        """Add a single route with optional auth requirement."""
        route = {"method": method, "path": path, "handler": handler, "require_auth": require_auth}
        if method not in self._routes:
            self._routes[method] = []
        self._routes[method].append(route)

    def handle_request(self, method: str, path: str) -> dict:
        """Handle an incoming request through middleware chain."""
        if self.state.auth_enabled:
            auth_result = auth.authenticate(path)
            if auth_result["status"] != "ok":
                return auth_result

        if not ratelimit.check_limit(self.state.rate_limit):
            return {"status": "error", "code": 429, "message": "rate limit exceeded"}

        for route in self._routes.get(method, []):
            if route["path"] == path:
                if route["require_auth"]:
                    auth_result = auth.authorize("read")
                    if auth_result["status"] != "ok":
                        return auth_result
                return route["handler"]()

        return {"status": "error", "code": 404, "message": "not found"}


def create_app(
    db_path: Optional[str] = None,
    auth_enabled: bool = True,
    rate_limit: int = 100,
) -> DevForgeApp:
    """Factory function to create and configure the application."""
    app = DevForgeApp(
        db_path=db_path,
        auth_enabled=auth_enabled,
        rate_limit=rate_limit,
    )
    app.register_routes()
    return app


if __name__ == "__main__":
    app = create_app()
    result = app.handle_request("GET", "/api/v1/users")
    print(result)
