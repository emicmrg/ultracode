# API Reference

## Authentication

All API requests require a Bearer token in the `Authorization` header:

```
Authorization: Bearer <jwt_token>
```

### POST /auth/login

Authenticate and receive a JWT token.

```json
// Request
{
  "email": "user@example.com",
  "password": "securepassword123"
}

// Response 200
{
  "token": "eyJhbGciOiJIUzI1NiJ9...",
  "user": {
    "id": "user-001",
    "email": "user@example.com",
    "name": "Example User",
    "role": "user"
  }
}

// Response 401
{
  "error": "invalid credentials"
}
```

### POST /auth/logout

Invalidate the current session token.

```
Authorization: Bearer <token>

// Response 200
{
  "message": "logged out"
}
```

## Users

### GET /api/v1/users

List all users (admin only).

```json
// Response 200
{
  "status": "ok",
  "data": [
    {
      "id": "admin-001",
      "email": "admin@devforge.io",
      "name": "Admin",
      "role": "admin"
    }
  ],
  "total": 1
}
```

### POST /api/v1/users

Create a new user account.

```json
// Request
{
  "email": "new@devforge.io",
  "name": "New User",
  "password": "SecurePass123!"
}

// Response 201
{
  "status": "ok",
  "data": {
    "id": "user-003",
    "email": "new@devforge.io",
    "name": "New User",
    "role": "user"
  }
}
```

## Projects

### GET /api/v1/projects

List projects with pagination and filtering.

```json
// Query Parameters
// ?user=<user_id>&visibility=public&page=1&limit=20

// Response 200
{
  "status": "ok",
  "data": [
    {
      "id": "proj-001",
      "name": "DevForge Core",
      "owner_id": "admin-001",
      "visibility": "public",
      "artifact_count": 42,
      "settings": {
        "max_artifacts": 200,
        "retention_days": 60,
        "default_branch": "main",
        "enable_ci": true
      }
    }
  ],
  "meta": {
    "limit": 20,
    "offset": 0,
    "total": 3,
    "has_more": false
  }
}
```

### POST /api/v1/projects

Create a new project.

```json
// Request
{
  "name": "My Project",
  "visibility": "private"
}

// Response 201
{
  "status": "ok",
  "data": {
    "id": "proj-004",
    "name": "My Project",
    "owner_id": "user-001",
    "visibility": "private"
  }
}
```

## Artifacts

### GET /api/v1/artifacts

List artifacts with optional project filter.

```json
// Query: ?project_id=proj-001

// Response 200
{
  "status": "ok",
  "data": [
    {
      "id": "art-001",
      "project_id": "proj-001",
      "name": "devforge-cli",
      "version": "1.2.0",
      "size": 1048576,
      "checksum": "sha256:a1b2c3d4e5f6...",
      "content_type": "application/octet-stream",
      "created_at": "2024-03-15T10:30:00Z"
    }
  ],
  "total": 3
}
```

### POST /api/v1/artifacts

Upload a new artifact. Uses multipart form data.

```json
// Response 201
{
  "status": "ok",
  "data": {
    "id": "art-004",
    "name": "my-library",
    "version": "1.0.0",
    "size": 524288,
    "checksum": "sha256:x9y0z1a2b3c4..."
  }
}
```

### GET /api/v1/artifacts/{id}/download

Stream an artifact for download. Returns binary content.

```
Response 200
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="devforge-cli-1.2.0.tar.gz"
Content-Length: 1048576
```

## Error Responses

All errors follow a consistent format:

```json
{
  "status": "error",
  "code": 400,
  "message": "Human-readable error description",
  "details": {
    "field": "email",
    "reason": "Invalid email format"
  }
}
```

### HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 201 | Created |
| 400 | Bad request / validation error |
| 401 | Unauthorized / missing token |
| 403 | Forbidden / missing scope |
| 404 | Resource not found |
| 409 | Conflict / duplicate |
| 429 | Rate limit exceeded |
| 500 | Internal server error |
| 503 | Service unavailable |
