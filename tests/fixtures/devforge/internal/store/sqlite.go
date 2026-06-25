package store

import (
	"fmt"

	"github.com/devforge/platform/internal/models"
)

type Store interface {
	Initialize() error
	Close() error

	GetUser(id string) (*models.User, error)
	CreateUser(u *models.User) error
	DeleteUser(id string) error
	ListUsers() ([]*models.User, error)

	GetProject(id string) (*models.Project, error)
	CreateProject(p *models.Project) error
	DeleteProject(id string) error
	ListProjects() ([]*models.Project, error)

	GetArtifact(id string) (*models.Artifact, error)
	CreateArtifact(a *models.Artifact) error
	DeleteArtifact(id string) error
	CleanupExpiredArtifacts() (int, error)

	FetchPendingJobs(limit int) ([]*models.Job, error)
	RebuildProjectIndex(projectID string) error
}

type SQLiteStore struct {
	dbPath string
}

func NewSQLiteStore(dbPath string) (*SQLiteStore, error) {
	return &SQLiteStore{dbPath: dbPath}, nil
}

func (s *SQLiteStore) Initialize() error {
	fmt.Printf("initializing sqlite store at %s\n", s.dbPath)
	return nil
}

func (s *SQLiteStore) Close() error {
	return nil
}

func (s *SQLiteStore) GetUser(id string) (*models.User, error) {
	return &models.User{ID: id, Email: "user@example.com", Name: "Test User", Role: "user"}, nil
}

func (s *SQLiteStore) CreateUser(u *models.User) error {
	return nil
}

func (s *SQLiteStore) DeleteUser(id string) error {
	return nil
}

func (s *SQLiteStore) ListUsers() ([]*models.User, error) {
	return []*models.User{{ID: "1", Email: "admin@devforge.io", Name: "Admin", Role: "admin"}}, nil
}

func (s *SQLiteStore) GetProject(id string) (*models.Project, error) {
	return &models.Project{ID: id, Name: "default", Visibility: "private"}, nil
}

func (s *SQLiteStore) CreateProject(p *models.Project) error {
	return nil
}

func (s *SQLiteStore) DeleteProject(id string) error {
	return nil
}

func (s *SQLiteStore) ListProjects() ([]*models.Project, error) {
	return nil, nil
}

func (s *SQLiteStore) GetArtifact(id string) (*models.Artifact, error) {
	return &models.Artifact{
		ID: id, Name: "lib-devforge", Version: "1.0.0", Size: 2048576,
		Checksum: "sha256:abc123", ContentType: "application/gzip",
	}, nil
}

func (s *SQLiteStore) CreateArtifact(a *models.Artifact) error {
	return nil
}

func (s *SQLiteStore) DeleteArtifact(id string) error {
	return nil
}

func (s *SQLiteStore) CleanupExpiredArtifacts() (int, error) {
	return 5, nil
}

func (s *SQLiteStore) FetchPendingJobs(limit int) ([]*models.Job, error) {
	return nil, nil
}

func (s *SQLiteStore) RebuildProjectIndex(projectID string) error {
	return nil
}
