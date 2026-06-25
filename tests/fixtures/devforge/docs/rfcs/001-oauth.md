# RFC 001: OAuth2 Provider Integration

**Status:** Draft
**Author:** DevForge Team
**Date:** 2024-05-01

## Summary

Add OAuth2 authentication support to allow users to sign in with GitHub, Google, and other providers.

## Motivation

Users want to sign in with their existing accounts instead of creating yet another password. Enterprise customers need SAML/OIDC for their identity providers.

## Design

### Provider Interface

```go
type OAuthProvider interface {
    Name() string
    AuthorizeURL(state string) string
    Exchange(ctx context.Context, code string) (*TokenResult, error)
    Refresh(ctx context.Context, token string) (*TokenResult, error)
    UserInfo(ctx context.Context, token string) (*UserInfo, error)
}
```

### Configuration

```yaml
auth:
  oauth:
    providers:
      - name: github
        client_id: "${GITHUB_CLIENT_ID}"
        client_secret: "${GITHUB_CLIENT_SECRET}"
        auth_url: "https://github.com/login/oauth/authorize"
        token_url: "https://github.com/login/oauth/access_token"
        userinfo_url: "https://api.github.com/user"
        scopes:
          - "read:user"
          - "user:email"
```

### Flow

1. User clicks "Sign in with GitHub"
2. Server generates state token, stores in session
3. User is redirected to GitHub authorization page
4. User approves, GitHub redirects back with code
5. Server validates state token
6. Server exchanges code for access token
7. Server fetches user info from provider
8. Server creates/links local user account
9. Server issues JWT for subsequent API calls

## Security Considerations

- State parameter prevents CSRF attacks
- PKCE flow for public clients (SPA)
- Token scopes are validated on every exchange
- Provider secrets stored in environment variables, never in code
- Redirect URIs must be exact match (no open redirect)

## Alternatives Considered

1. **Only email/password**: Rejected — user demand for SSO
2. **Custom OAuth server**: Rejected — unnecessary complexity for MVP
3. **Auth0/Okta/FusionAuth**: Considered for enterprise, deferred to v2

## Open Questions

- How do we handle provider account linking when email differs?
- Should we support multiple providers per user account?
- Do we need a fallback for when providers are down?

## Implementation Plan

1. Add OAuth2Client to `internal/auth/oauth.go`
2. Update configuration schema in `config/default.yaml`
3. Add OAuth routes to `cmd/server/main.go`
4. Store provider metadata in database
5. Add OAuth section to API reference docs
6. Write integration tests with mock provider
