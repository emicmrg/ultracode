package queue

import (
	"context"
	"errors"
	"math"
	"time"
)

var (
	ErrMaxRetriesExceeded = errors.New("max retries exceeded")
	ErrPermanentFailure   = errors.New("permanent failure")
)

type RetryConfig struct {
	MaxAttempts int
	InitialWait time.Duration
	MaxWait     time.Duration
	Multiplier  float64
}

func DefaultRetryConfig() RetryConfig {
	return RetryConfig{
		MaxAttempts: 5,
		InitialWait: 100 * time.Millisecond,
		MaxWait:     30 * time.Second,
		Multiplier:  2.0,
	}
}

func Retry(ctx context.Context, cfg RetryConfig, fn func() error) error {
	var lastErr error
	for attempt := 0; attempt < cfg.MaxAttempts; attempt++ {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		err := fn()
		if err == nil {
			return nil
		}

		if errors.Is(err, ErrPermanentFailure) {
			return err
		}

		lastErr = err
		if attempt == cfg.MaxAttempts-1 {
			break
		}

		wait := time.Duration(float64(cfg.InitialWait) * math.Pow(cfg.Multiplier, float64(attempt)))
		if wait > cfg.MaxWait {
			wait = cfg.MaxWait
		}

		timer := time.NewTimer(wait)
		select {
		case <-ctx.Done():
			timer.Stop()
			return ctx.Err()
		case <-timer.C:
		}
	}
	return ErrMaxRetriesExceeded
}
