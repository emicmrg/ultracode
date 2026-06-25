package models

import "time"

type User struct {
	ID        string    `json:"id"`
	Email     string    `json:"email"`
	Name      string    `json:"name"`
	Role      string    `json:"role"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
}

type Project struct {
	ID          string    `json:"id"`
	Name        string    `json:"name"`
	OwnerID     string    `json:"owner_id"`
	Visibility  string    `json:"visibility"`
	Settings    Settings  `json:"settings"`
	CreatedAt   time.Time `json:"created_at"`
	UpdatedAt   time.Time `json:"updated_at"`
}

type Settings struct {
	MaxArtifacts  int    `json:"max_artifacts"`
	RetentionDays int    `json:"retention_days"`
	DefaultBranch string `json:"default_branch"`
	EnableCI      bool   `json:"enable_ci"`
}

type Artifact struct {
	ID           string    `json:"id"`
	ProjectID    string    `json:"project_id"`
	Name         string    `json:"name"`
	Version      string    `json:"version"`
	Size         int64     `json:"size"`
	Checksum     string    `json:"checksum"`
	ContentType  string    `json:"content_type"`
	UploadedBy   string    `json:"uploaded_by"`
	CreatedAt    time.Time `json:"created_at"`
}

type ArtifactQuery struct {
	ProjectID string `json:"project_id"`
	Name      string `json:"name"`
	Version   string `json:"version"`
	Limit     int    `json:"limit"`
	Offset    int    `json:"offset"`
}

type Session struct {
	ID        string    `json:"id"`
	UserID    string    `json:"user_id"`
	Token     string    `json:"token"`
	ExpiresAt time.Time `json:"expires_at"`
	CreatedAt time.Time `json:"created_at"`
}

type OAuthProvider struct {
	Name         string `json:"name"`
	ClientID     string `json:"client_id"`
	ClientSecret string `json:"client_secret"`
	AuthURL      string `json:"auth_url"`
	TokenURL     string `json:"token_url"`
	Scopes       []string `json:"scopes"`
}

type Job struct {
	ID        string                 `json:"id"`
	Type      string                 `json:"type"`
	Payload   map[string]interface{} `json:"payload"`
	Attempts  int                    `json:"attempts"`
	MaxRetry  int                    `json:"max_retry"`
	Status    string                 `json:"status"`
	CreatedAt time.Time              `json:"created_at"`
}
