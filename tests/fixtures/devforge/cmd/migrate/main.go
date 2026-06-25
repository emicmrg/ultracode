package main

import (
	"database/sql"
	"flag"
	"fmt"
	"log"
	"os"
)

type Migration struct {
	Version     int
	Description string
	Up          string
	Down        string
}

var migrations = []Migration{
	{
		Version:     1,
		Description: "create users table",
		Up: `CREATE TABLE IF NOT EXISTS users (
			id TEXT PRIMARY KEY,
			email TEXT NOT NULL UNIQUE,
			name TEXT NOT NULL,
			role TEXT NOT NULL DEFAULT 'user',
			created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
			updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
		)`,
		Down: "DROP TABLE IF EXISTS users",
	},
	{
		Version:     2,
		Description: "create projects table",
		Up: `CREATE TABLE IF NOT EXISTS projects (
			id TEXT PRIMARY KEY,
			name TEXT NOT NULL,
			owner_id TEXT NOT NULL,
			visibility TEXT DEFAULT 'private',
			created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
			updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
			FOREIGN KEY (owner_id) REFERENCES users(id)
		)`,
		Down: "DROP TABLE IF EXISTS projects",
	},
	{
		Version:     3,
		Description: "create artifacts table",
		Up: `CREATE TABLE IF NOT EXISTS artifacts (
			id TEXT PRIMARY KEY,
			project_id TEXT NOT NULL,
			name TEXT NOT NULL,
			version TEXT NOT NULL,
			size INTEGER DEFAULT 0,
			checksum TEXT,
			content_type TEXT DEFAULT 'application/octet-stream',
			uploaded_by TEXT,
			created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
			FOREIGN KEY (project_id) REFERENCES projects(id)
		)`,
		Down: "DROP TABLE IF EXISTS artifacts",
	},
	{
		Version:     4,
		Description: "add settings column to projects",
		Up:          "ALTER TABLE projects ADD COLUMN settings TEXT DEFAULT '{}'",
		Down:        "ALTER TABLE projects DROP COLUMN settings",
	},
}

func validate(db *sql.DB) error {
	_, err := db.Exec(`CREATE TABLE IF NOT EXISTS schema_migrations (
		version INTEGER PRIMARY KEY,
		applied_at DATETIME DEFAULT CURRENT_TIMESTAMP
	)`)
	return err
}

func getAppliedVersions(db *sql.DB) (map[int]bool, error) {
	rows, err := db.Query("SELECT version FROM schema_migrations ORDER BY version")
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	applied := make(map[int]bool)
	for rows.Next() {
		var v int
		if err := rows.Scan(&v); err != nil {
			return nil, err
		}
		applied[v] = true
	}
	return applied, rows.Err()
}

func applyMigration(db *sql.DB, m Migration, direction string) error {
	sql := m.Up
	if direction == "down" {
		sql = m.Down
	}

	tx, err := db.Begin()
	if err != nil {
		return err
	}
	defer tx.Rollback()

	if _, err := tx.Exec(sql); err != nil {
		return fmt.Errorf("migration %d (%s): %w", m.Version, m.Description, err)
	}

	if direction == "up" {
		if _, err := tx.Exec("INSERT INTO schema_migrations (version) VALUES (?)", m.Version); err != nil {
			return err
		}
	} else {
		if _, err := tx.Exec("DELETE FROM schema_migrations WHERE version = ?", m.Version); err != nil {
			return err
		}
	}

	return tx.Commit()
}

func main() {
	dbPath := flag.String("db", "devforge.db", "database path")
	direction := flag.String("direction", "up", "migration direction: up or down")
	flag.Parse()

	db, err := sql.Open("sqlite3", *dbPath)
	if err != nil {
		log.Fatalf("cannot open database: %v", err)
	}
	defer db.Close()

	if err := validate(db); err != nil {
		log.Fatalf("cannot prepare schema_migrations: %v", err)
	}

	applied, err := getAppliedVersions(db)
	if err != nil {
		log.Fatalf("cannot read applied versions: %v", err)
	}

	for _, m := range migrations {
		if *direction == "up" && !applied[m.Version] {
			log.Printf("applying migration %d: %s", m.Version, m.Description)
			if err := applyMigration(db, m, "up"); err != nil {
				log.Fatalf("migration failed: %v", err)
			}
		} else if *direction == "down" && applied[m.Version] {
			log.Printf("reverting migration %d: %s", m.Version, m.Description)
			if err := applyMigration(db, m, "down"); err != nil {
				log.Fatalf("revert failed: %v", err)
			}
		}
	}

	fmt.Println("migrations complete")
	os.Exit(0)
}
