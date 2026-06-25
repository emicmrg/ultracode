package queue

import (
	"log"
	"sync"
	"sync/atomic"
	"time"

	"github.com/devforge/platform/internal/models"
)

type JobHandler func(job *models.Job) error

type Dispatcher struct {
	concurrency int
	maxRetry    int
	handler     JobHandler
	jobCh       chan *models.Job
	wg          sync.WaitGroup
	running     atomic.Bool
	mu          sync.Mutex
	active      atomic.Int32
}

func NewDispatcher(concurrency, maxRetry int, handler JobHandler) *Dispatcher {
	return &Dispatcher{
		concurrency: concurrency,
		maxRetry:    maxRetry,
		handler:     handler,
		jobCh:       make(chan *models.Job, concurrency*2),
	}
}

func (d *Dispatcher) Start() {
	d.running.Store(true)
	for i := 0; i < d.concurrency; i++ {
		d.wg.Add(1)
		go d.Worker(i)
	}
}

func (d *Dispatcher) Worker(id int) {
	defer d.wg.Done()
	for job := range d.jobCh {
		d.active.Add(1)
		d.processJob(job)
		d.active.Add(-1)
	}
}

func (d *Dispatcher) processJob(job *models.Job) {
	for attempt := 0; attempt <= d.maxRetry; attempt++ {
		job.Attempts = attempt + 1
		err := d.handler(job)
		if err == nil {
			job.Status = "completed"
			return
		}
		log.Printf("job %s attempt %d/%d failed: %v", job.ID, job.Attempts, d.maxRetry+1, err)
		if attempt < d.maxRetry {
			backoff := ExponentialBackoff(attempt)
			time.Sleep(backoff)
		}
	}
	job.Status = "failed"
}

func (d *Dispatcher) Dispatch(job *models.Job) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.running.Load() {
		d.jobCh <- job
	}
}

func (d *Dispatcher) Shutdown() {
	d.running.Store(false)
	close(d.jobCh)
	d.wg.Wait()
}

func (d *Dispatcher) ActiveJobs() int {
	return int(d.active.Load())
}

func ExponentialBackoff(attempt int) time.Duration {
	base := 100 * time.Millisecond
	multiplier := time.Duration(1 << attempt)
	return base * multiplier
}
