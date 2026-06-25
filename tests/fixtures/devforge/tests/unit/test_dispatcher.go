package tests_test

import (
	"testing"
	"time"

	"github.com/devforge/platform/internal/models"
	"github.com/devforge/platform/internal/queue"
)

func TestNewDispatcher(t *testing.T) {
	d := queue.NewDispatcher(4, 2, func(job *models.Job) error {
		return nil
	})
	if d == nil {
		t.Fatal("dispatcher should not be nil")
	}
}

func TestDispatcherDispatch(t *testing.T) {
	processed := make(chan string, 1)
	d := queue.NewDispatcher(1, 1, func(job *models.Job) error {
		processed <- job.ID
		return nil
	})
	d.Start()
	defer d.Shutdown()

	job := &models.Job{
		ID:      "job-001",
		Type:    "test",
		Payload: map[string]interface{}{"key": "value"},
	}

	d.Dispatch(job)

	select {
	case id := <-processed:
		if id != "job-001" {
			t.Errorf("expected job-001, got %s", id)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("timeout waiting for job processing")
	}
}

func TestDispatcherActiveJobs(t *testing.T) {
	started := make(chan struct{})
	done := make(chan struct{})

	d := queue.NewDispatcher(2, 0, func(job *models.Job) error {
		started <- struct{}{}
		<-done
		return nil
	})
	d.Start()
	defer func() { close(done); d.Shutdown() }()

	d.Dispatch(&models.Job{ID: "j1", Type: "block"})
	d.Dispatch(&models.Job{ID: "j2", Type: "block"})

	<-started
	<-started

	active := d.ActiveJobs()
	if active != 2 {
		t.Errorf("expected 2 active jobs, got %d", active)
	}
}

func TestExponentialBackoff(t *testing.T) {
	tests := []struct {
		attempt  int
		expected time.Duration
	}{
		{0, 100 * time.Millisecond},
		{1, 200 * time.Millisecond},
		{2, 400 * time.Millisecond},
		{3, 800 * time.Millisecond},
		{4, 1600 * time.Millisecond},
	}

	for _, tc := range tests {
		result := queue.ExponentialBackoff(tc.attempt)
		if result != tc.expected {
			t.Errorf("attempt %d: expected %v, got %v", tc.attempt, tc.expected, result)
		}
	}
}

func TestDispatcherMaxRetry(t *testing.T) {
	attempts := make(chan int, 100)
	maxRetry := 2

	d := queue.NewDispatcher(1, maxRetry, func(job *models.Job) error {
		attempts <- job.Attempts
		return queue.ErrPermanentFailure
	})
	d.Start()
	defer d.Shutdown()

	d.Dispatch(&models.Job{ID: "retry-test", Type: "failing"})

	time.Sleep(200 * time.Millisecond)
	close(attempts)

	count := 0
	for range attempts {
		count++
	}

	if count != 1 {
		t.Errorf("expected 1 attempt (PermanentFailure), got %d", count)
	}
}
