package auth

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/devforge/platform/internal/models"
)

var (
	ErrSessionExpired  = fmt.Errorf("session expired")
	ErrSessionNotFound = fmt.Errorf("session not found")
	ErrInvalidToken    = fmt.Errorf("invalid session token")
)

type SessionManager struct {
	store       SessionStore
	maxLifetime time.Duration
}

type SessionStore interface {
	GetSession(id string) (*models.Session, error)
	SaveSession(s *models.Session) error
	DeleteSession(id string) error
}

func NewSessionManager(store SessionStore, maxLifetime time.Duration) *SessionManager {
	return &SessionManager{
		store:       store,
		maxLifetime: maxLifetime,
	}
}

func (sm *SessionManager) NewSession(userID string) (*models.Session, error) {
	token, err := generateToken(48)
	if err != nil {
		return nil, fmt.Errorf("generate token: %w", err)
	}

	session := &models.Session{
		ID:        token[:16],
		UserID:    userID,
		Token:     token,
		ExpiresAt: time.Now().Add(sm.maxLifetime),
		CreatedAt: time.Now(),
	}

	if err := sm.store.SaveSession(session); err != nil {
		return nil, err
	}
	return session, nil
}

func (sm *SessionManager) Validate(token string) (*models.Session, error) {
	sessionID := extractSessionID(token)
	if sessionID == "" {
		return nil, ErrInvalidToken
	}

	session, err := sm.store.GetSession(sessionID)
	if err != nil {
		return nil, ErrSessionNotFound
	}
	if time.Now().After(session.ExpiresAt) {
		return nil, ErrSessionExpired
	}
	if session.Token != token {
		return nil, ErrInvalidToken
	}
	return session, nil
}

func (sm *SessionManager) Invalidate(token string) error {
	sessionID := extractSessionID(token)
	if sessionID == "" {
		return ErrInvalidToken
	}
	return sm.store.DeleteSession(sessionID)
}

func extractSessionID(token string) string {
	if len(token) < 16 {
		return ""
	}
	return token[:16]
}

func generateToken(length int) (string, error) {
	bytes := make([]byte, length)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return hex.EncodeToString(bytes), nil
}

func Middleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		authHeader := r.Header.Get("Authorization")
		if authHeader == "" {
			http.Error(w, `{"error":"missing authorization header"}`, http.StatusUnauthorized)
			return
		}

		parts := strings.SplitN(authHeader, " ", 2)
		if len(parts) != 2 || parts[0] != "Bearer" {
			http.Error(w, `{"error":"invalid authorization format"}`, http.StatusUnauthorized)
			return
		}

		if parts[1] == "" {
			http.Error(w, `{"error":"empty token"}`, http.StatusUnauthorized)
			return
		}

		next.ServeHTTP(w, r)
	})
}
