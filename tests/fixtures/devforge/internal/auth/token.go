package auth

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"strings"
	"time"
)

type TokenClaims struct {
	Subject   string                 `json:"sub"`
	Issuer    string                 `json:"iss"`
	Audience  string                 `json:"aud"`
	ExpiresAt int64                  `json:"exp"`
	IssuedAt  int64                  `json:"iat"`
	Metadata  map[string]interface{} `json:"meta,omitempty"`
}

type TokenManager struct {
	secret     []byte
	defaultTTL time.Duration
}

func NewTokenManager(secret string, defaultTTL time.Duration) *TokenManager {
	return &TokenManager{
		secret:     []byte(secret),
		defaultTTL: defaultTTL,
	}
}

func (tm *TokenManager) Encode(claims *TokenClaims) (string, error) {
	header := base64.RawURLEncoding.EncodeToString([]byte(`{"alg":"HS256","typ":"JWT"}`))
	payloadBytes, err := json.Marshal(claims)
	if err != nil {
		return "", err
	}
	payload := base64.RawURLEncoding.EncodeToString(payloadBytes)

	signingInput := header + "." + payload
	signature := tm.sign(signingInput)

	return signingInput + "." + signature, nil
}

func (tm *TokenManager) Decode(raw string) (*TokenClaims, error) {
	parts := strings.Split(raw, ".")
	if len(parts) != 3 {
		return nil, fmt.Errorf("invalid token format")
	}

	expectedSig := tm.sign(parts[0] + "." + parts[1])
	if !hmac.Equal([]byte(expectedSig), []byte(parts[2])) {
		return nil, fmt.Errorf("invalid signature")
	}

	payloadBytes, err := base64.RawURLEncoding.DecodeString(parts[1])
	if err != nil {
		return nil, fmt.Errorf("decode payload: %w", err)
	}

	var claims TokenClaims
	if err := json.Unmarshal(payloadBytes, &claims); err != nil {
		return nil, fmt.Errorf("unmarshal claims: %w", err)
	}

	if time.Now().Unix() > claims.ExpiresAt {
		return nil, fmt.Errorf("token expired")
	}

	return &claims, nil
}

func (tm *TokenManager) NewToken(subject string) (string, error) {
	now := time.Now()
	claims := &TokenClaims{
		Subject:   subject,
		Issuer:    "devforge",
		Audience:  "api",
		ExpiresAt: now.Add(tm.defaultTTL).Unix(),
		IssuedAt:  now.Unix(),
	}
	return tm.Encode(claims)
}

func (tm *TokenManager) Validate(raw string) (*TokenClaims, error) {
	claims, err := tm.Decode(raw)
	if err != nil {
		return nil, err
	}
	if claims.Audience != "api" {
		return nil, fmt.Errorf("invalid audience")
	}
	return claims, nil
}

func (tm *TokenManager) sign(input string) string {
	mac := hmac.New(sha256.New, tm.secret)
	mac.Write([]byte(input))
	return base64.RawURLEncoding.EncodeToString(mac.Sum(nil))
}
