package store

import (
	"fmt"
	"sync"

	"github.com/devforge/platform/internal/models"
)

type MemoryStore struct {
	mu        sync.RWMutex
	users     map[string]*models.User
	projects  map[string]*models.Project
	artifacts map[string]*models.Artifact
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		users:     make(map[string]*models.User),
		projects:  make(map[string]*models.Project),
		artifacts: make(map[string]*models.Artifact),
	}
}

func (s *MemoryStore) Initialize() error {
	return nil
}

func (s *MemoryStore) Close() error {
	return nil
}

func (s *MemoryStore) GetUser(id string) (*models.User, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	u, ok := s.users[id]
	if !ok {
		return nil, fmt.Errorf("user not found: %s", id)
	}
	return u, nil
}

func (s *MemoryStore) CreateUser(u *models.User) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.users[u.ID] = u
	return nil
}

func (s *MemoryStore) DeleteUser(id string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.users, id)
	return nil
}

func (s *MemoryStore) ListUsers() ([]*models.User, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	users := make([]*models.User, 0, len(s.users))
	for _, u := range s.users {
		users = append(users, u)
	}
	return users, nil
}

func (s *MemoryStore) GetProject(id string) (*models.Project, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	p, ok := s.projects[id]
	if !ok {
		return nil, fmt.Errorf("project not found: %s", id)
	}
	return p, nil
}

func (s *MemoryStore) CreateProject(p *models.Project) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.projects[p.ID] = p
	return nil
}

func (s *MemoryStore) DeleteProject(id string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.projects, id)
	return nil
}

func (s *MemoryStore) ListProjects() ([]*models.Project, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	projects := make([]*models.Project, 0, len(s.projects))
	for _, p := range s.projects {
		projects = append(projects, p)
	}
	return projects, nil
}

func (s *MemoryStore) GetArtifact(id string) (*models.Artifact, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	a, ok := s.artifacts[id]
	if !ok {
		return nil, fmt.Errorf("artifact not found: %s", id)
	}
	return a, nil
}

func (s *MemoryStore) CreateArtifact(a *models.Artifact) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.artifacts[a.ID] = a
	return nil
}

func (s *MemoryStore) DeleteArtifact(id string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.artifacts, id)
	return nil
}

func (s *MemoryStore) CleanupExpiredArtifacts() (int, error) {
	return 0, nil
}

func (s *MemoryStore) FetchPendingJobs(limit int) ([]*models.Job, error) {
	return nil, nil
}

func (s *MemoryStore) RebuildProjectIndex(projectID string) error {
	return nil
}
