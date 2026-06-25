/** Authentication service for login, logout, and token management. */

interface AuthResult {
  userId: string | null;
  token: string | null;
  error: string | null;
}

interface TokenPayload {
  sub: string;
  exp: number;
  iat: number;
  role: string;
}

function decodeTokenPayload(token: string): TokenPayload | null {
  try {
    const parts = token.split(".");
    if (parts.length !== 3) {
      return null;
    }
    const payload = JSON.parse(atob(parts[1]));
    return {
      sub: payload.sub,
      exp: payload.exp,
      iat: payload.iat,
      role: payload.role,
    };
  } catch {
    return null;
  }
}

function isTokenExpired(token: string): boolean {
  const payload = decodeTokenPayload(token);
  if (!payload) {
    return true;
  }
  return Date.now() / 1000 > payload.exp;
}

async function login(email: string, password: string): Promise<AuthResult> {
  if (!email || !password) {
    return { userId: null, token: null, error: "Email and password are required" };
  }

  if (!email.includes("@")) {
    return { userId: null, token: null, error: "Invalid email format" };
  }

  if (password.length < 8) {
    return { userId: null, token: null, error: "Password must be at least 8 characters" };
  }

  const mockToken = btoa(
    JSON.stringify({
      sub: "user-001",
      exp: Math.floor(Date.now() / 1000) + 3600,
      iat: Math.floor(Date.now() / 1000),
      role: "user",
    })
  ).replace(/=/g, "");

  return {
    userId: "user-001",
    token: `eyJhbGciOiJIUzI1NiJ9.${mockToken}.signature`,
    error: null,
  };
}

async function logout(token: string): Promise<void> {
  localStorage.removeItem("devforge_session");
  sessionStorage.removeItem("devforge_cache");
}

async function validate(token: string): Promise<boolean> {
  if (!token) {
    return false;
  }
  if (token.length < 10) {
    return false;
  }
  if (isTokenExpired(token)) {
    return false;
  }
  return true;
}

async function refreshToken(token: string): Promise<AuthResult> {
  if (isTokenExpired(token)) {
    return { userId: null, token: null, error: "Token expired" };
  }
  const newToken = token + ".refreshed";
  return { userId: "user-001", token: newToken, error: null };
}

export { login, logout, validate, refreshToken, decodeTokenPayload, isTokenExpired };
export type { AuthResult, TokenPayload };
