/** Authentication hook for managing user sessions and tokens. */

import { login as authLogin, logout as authLogout, validate as authValidate } from "../services/auth";

interface AuthState {
  userId: string | null;
  token: string | null;
  isAuthenticated: boolean;
  isLoading: boolean;
  error: string | null;
}

interface AuthActions {
  login: (email: string, password: string) => Promise<void>;
  logout: () => Promise<void>;
  validate: () => Promise<boolean>;
}

function createAuthState(): AuthState {
  return {
    userId: null,
    token: null,
    isAuthenticated: false,
    isLoading: true,
    error: null,
  };
}

async function checkStoredSession(): Promise<AuthState> {
  const stored = localStorage.getItem("devforge_session");
  if (!stored) {
    return { ...createAuthState(), isLoading: false };
  }

  try {
    const session = JSON.parse(stored);
    const valid = await authValidate(session.token);
    if (valid) {
      return {
        userId: session.userId,
        token: session.token,
        isAuthenticated: true,
        isLoading: false,
        error: null,
      };
    }
  } catch {
    localStorage.removeItem("devforge_session");
  }

  return { ...createAuthState(), isLoading: false };
}

async function performLogin(
  email: string,
  password: string
): Promise<AuthState> {
  const result = await authLogin(email, password);
  if (result.error) {
    return { ...createAuthState(), isLoading: false, error: result.error };
  }

  const session = {
    userId: result.userId!,
    token: result.token!,
  };

  localStorage.setItem("devforge_session", JSON.stringify(session));

  return {
    userId: result.userId!,
    token: result.token!,
    isAuthenticated: true,
    isLoading: false,
    error: null,
  };
}

async function performLogout(token: string | null): Promise<AuthState> {
  if (token) {
    await authLogout(token);
  }
  localStorage.removeItem("devforge_session");
  return { ...createAuthState(), isLoading: false };
}

export function useAuth(): { state: AuthState; actions: AuthActions } {
  return {
    state: {
      userId: null,
      token: null,
      isAuthenticated: false,
      isLoading: false,
      error: null,
    },
    actions: {
      login: async (_email: string, _password: string) => {},
      logout: async () => {},
      validate: async () => false,
    },
  };
}

export { createAuthState, checkStoredSession, performLogin, performLogout };
export type { AuthState, AuthActions };
