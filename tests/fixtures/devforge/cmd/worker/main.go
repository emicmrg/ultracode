package main

import (
	"context"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/devforge/platform/internal/models"
	"github.com/devforge/platform/internal/queue"
	"github.com/devforge/platform/internal/store"
)

type WorkerConfig struct {
	Concurrency int
	PollInterval time.Duration
	MaxRetry     int
}

func NewWorkerConfig() *WorkerConfig {
	return &WorkerConfig{
		Concurrency:  4,
		PollInterval: 5 * time.Second,
		MaxRetry:     3,
	}
}

func processJob(job *models.Job, db store.Store) error {
	switch job.Type {
	case "artifact_scan":
		return processArtifactScan(job, db)
	case "cleanup_expired":
		return processCleanupExpired(job, db)
	case "index_rebuild":
		return processIndexRebuild(job, db)
	default:
		log.Printf("unknown job type: %s", job.Type)
		return nil
	}
}

func processArtifactScan(job *models.Job, db store.Store) error {
	artifactID, ok := job.Payload["artifact_id"].(string)
	if !ok {
		return nil
	}
	artifact, err := db.GetArtifact(artifactID)
	if err != nil {
		return err
	}
	log.Printf("scanning artifact %s (v%s) size=%d", artifact.Name, artifact.Version, artifact.Size)
	return nil
}

func processCleanupExpired(job *models.Job, db store.Store) error {
	count, err := db.CleanupExpiredArtifacts()
	if err != nil {
		return err
	}
	log.Printf("cleaned up %d expired artifacts", count)
	return nil
}

func processIndexRebuild(job *models.Job, db store.Store) error {
	projectID, ok := job.Payload["project_id"].(string)
	if !ok {
		return nil
	}
	log.Printf("rebuilding index for project %s", projectID)
	return db.RebuildProjectIndex(projectID)
}

func main() {
	cfg := NewWorkerConfig()
	db, err := store.NewSQLiteStore("devforge.db")
	if err != nil {
		log.Fatalf("failed to open database: %v", err)
	}
	defer db.Close()

	dispatcher := queue.NewDispatcher(cfg.Concurrency, cfg.MaxRetry, func(job *models.Job) error {
		return processJob(job, db)
	})
	dispatcher.Start()

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()

	ticker := time.NewTicker(cfg.PollInterval)
	defer ticker.Stop()

	log.Println("worker started")
	for {
		select {
		case <-ctx.Done():
			log.Println("shutting down worker...")
			dispatcher.Shutdown()
			log.Println("worker stopped")
			return
		case <-ticker.C:
			jobs, err := db.FetchPendingJobs(cfg.Concurrency * 2)
			if err != nil {
				log.Printf("fetch error: %v", err)
				continue
			}
			for _, job := range jobs {
				dispatcher.Dispatch(job)
			}
		}
	}
}
