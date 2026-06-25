# RFC 002: Event System and WebHooks

**Status:** Draft
**Author:** DevForge Team
**Date:** 2024-05-20

## Summary

Introduce an event-driven architecture to enable real-time notifications and webhook integrations.

## Motivation

Users need to react to artifact uploads, scans, and deletions. CI/CD pipelines need to trigger on new artifact versions. The current polling-based approach is inefficient and introduces latency.

## Design

### Event Types

```go
type EventType string

const (
    EventArtifactUploaded  EventType = "artifact.uploaded"
    EventArtifactScanned   EventType = "artifact.scanned"
    EventArtifactDeleted   EventType = "artifact.deleted"
    EventProjectCreated    EventType = "project.created"
    EventProjectDeleted    EventType = "project.deleted"
    EventUserRegistered    EventType = "user.registered"
)
```

### Event Structure

```go
type Event struct {
    ID        string          `json:"id"`
    Type      EventType       `json:"type"`
    Source    string          `json:"source"`
    Timestamp time.Time       `json:"timestamp"`
    Actor     string          `json:"actor"`
    Data      json.RawMessage `json:"data"`
}
```

### WebHook Configuration

```go
type WebHook struct {
    ID       string   `json:"id"`
    URL      string   `json:"url"`
    Events   []string `json:"events"`
    Secret   string   `json:"secret"`
    Active   bool     `json:"active"`
}
```

### Delivery

WebHook deliveries use HMAC-SHA256 signatures for verification:

```
X-DevForge-Event: artifact.uploaded
X-DevForge-Delivery: abc-123-def
X-DevForge-Signature: sha256=abcd1234...
```

## Architecture

```
Event Producer ──▶ Event Bus (Redis streams) ──▶ WebHook Dispatcher
                                                      │
                                                      ├──▶ WebHook 1
                                                      ├──▶ WebHook 2
                                                      └──▶ WebSocket (Dashboard)
```

## Alternatives Considered

1. **Direct HTTP calls**: Rejected — no retry, no batching
2. **Kafka**: Rejected — too heavy for initial deployment
3. **NATS**: Considered for v2 when clustering is needed

## Open Questions

- Maximum payload size for events?
- Retry strategy for failed deliveries? (exponential backoff, max 5 attempts)
- Event retention period? (7 days default)
- Should we support event filtering at subscription level?
