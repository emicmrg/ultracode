"""Rate limiting middleware using token bucket algorithm."""

import time
from typing import Optional


class RateLimiter:
    """Token bucket based rate limiter."""

    def __init__(
        self,
        rate: int = 100,
        burst: int = 200,
        window_seconds: float = 60.0,
    ) -> None:
        self.rate = rate
        self.burst = burst
        self.window = window_seconds
        self._tokens: float = float(burst)
        self._last_refill: float = time.monotonic()
        self._request_count: int = 0

    def _refill(self) -> None:
        """Refill tokens based on elapsed time."""
        now = time.monotonic()
        elapsed = now - self._last_refill
        new_tokens = elapsed * (self.rate / self.window)
        self._tokens = min(float(self.burst), self._tokens + new_tokens)
        self._last_refill = now

    def allow(self) -> bool:
        """Check if a request is allowed under the current rate."""
        self._refill()
        if self._tokens >= 1.0:
            self._tokens -= 1.0
            self._request_count += 1
            return True
        return False

    def reset(self) -> None:
        """Reset the limiter to initial state."""
        self._tokens = float(self.burst)
        self._last_refill = time.monotonic()
        self._request_count = 0


_default_limiter: Optional[RateLimiter] = None


def get_limiter() -> RateLimiter:
    """Get or create the default rate limiter."""
    global _default_limiter
    if _default_limiter is None:
        _default_limiter = RateLimiter(rate=100, burst=200)
    return _default_limiter


def check_limit(max_requests: int = 100) -> bool:
    """Check rate limit before processing request."""
    limiter = get_limiter()
    if limiter.rate != max_requests:
        limiter.rate = max_requests
    return limiter.allow()
