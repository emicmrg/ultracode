package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/devforge/platform/internal/auth"
	"github.com/devforge/platform/internal/models"
	"github.com/devforge/platform/internal/store"
)

type ServerConfig struct {
	Port            int
	ReadTimeout     time.Duration
	WriteTimeout    time.Duration
	ShutdownTimeout time.Duration
	AuthEnabled     bool
}

func NewServerConfig() *ServerConfig {
	return &ServerConfig{
		Port:            8080,
		ReadTimeout:     15 * time.Second,
		WriteTimeout:    30 * time.Second,
		ShutdownTimeout: 10 * time.Second,
		AuthEnabled:     true,
	}
}

func parseFlags(cfg *ServerConfig) {
	flag.IntVar(&cfg.Port, "port", cfg.Port, "server port")
	flag.DurationVar(&cfg.ReadTimeout, "read-timeout", cfg.ReadTimeout, "read timeout")
	flag.DurationVar(&cfg.WriteTimeout, "write-timeout", cfg.WriteTimeout, "write timeout")
	flag.BoolVar(&cfg.AuthEnabled, "auth", cfg.AuthEnabled, "enable authentication")
	flag.Parse()
}

func registerRoutes(mux *http.ServeMux, db store.Store) {
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		fmt.Fprintf(w, `{"status":"ok"}`)
	})

	mux.HandleFunc("/api/v1/users", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodGet {
			users, err := db.ListUsers()
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			fmt.Fprintf(w, `{"users":%v}`, len(users))
		}
	})

	mux.HandleFunc("/api/v1/projects", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodPost {
			var p models.Project
			p.Name = "new-project"
			p.Visibility = "private"
			p.Settings = models.Settings{
				MaxArtifacts:  100,
				RetentionDays: 90,
				DefaultBranch: "main",
				EnableCI:      true,
			}
			if err := db.CreateProject(&p); err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			fmt.Fprintf(w, `{"id":"%s","name":"%s"}`, p.ID, p.Name)
		}
	})
}

func main() {
	cfg := NewServerConfig()
	parseFlags(cfg)

	db, err := store.NewSQLiteStore("devforge.db")
	if err != nil {
		log.Fatalf("failed to open database: %v", err)
	}
	defer db.Close()

	if err := db.Initialize(); err != nil {
		log.Fatalf("failed to initialize database: %v", err)
	}

	mux := http.NewServeMux()
	registerRoutes(mux, db)

	var handler http.Handler = mux
	if cfg.AuthEnabled {
		handler = auth.Middleware(mux)
	}

	srv := &http.Server{
		Addr:         fmt.Sprintf(":%d", cfg.Port),
		Handler:      handler,
		ReadTimeout:  cfg.ReadTimeout,
		WriteTimeout: cfg.WriteTimeout,
	}

	go func() {
		log.Printf("server listening on :%d", cfg.Port)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("server error: %v", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	log.Println("shutting down server...")

	ctx, cancel := context.WithTimeout(context.Background(), cfg.ShutdownTimeout)
	defer cancel()
	if err := srv.Shutdown(ctx); err != nil {
		log.Fatalf("forced shutdown: %v", err)
	}
	log.Println("server stopped")
}
