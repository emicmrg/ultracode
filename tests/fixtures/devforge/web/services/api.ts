/** API service with generic fetch wrapper and error handling. */

import { validateApiResponse } from "../utils/validation";

const API_BASE = "/api/v1";

interface ApiResponse<T> {
  data: T | null;
  error: string | null;
  status: number;
}

interface Project {
  id: string;
  name: string;
  ownerId: string;
  visibility: "public" | "private" | "internal";
  artifactCount: number;
  totalSize: number;
  updatedAt: Date;
  createdAt: Date;
}

interface Artifact {
  id: string;
  projectId: string;
  name: string;
  version: string;
  size: number;
  checksum: string;
  contentType: string;
  uploadedBy: string;
  createdAt: Date;
}

interface User {
  id: string;
  email: string;
  name: string;
  role: string;
}

async function request<T>(
  endpoint: string,
  options: RequestInit = {}
): Promise<ApiResponse<T>> {
  const url = `${API_BASE}${endpoint}`;
  const headers: Record<string, string> = {
    "Content-Type": "application/json",
    ...(options.headers as Record<string, string>),
  };

  const token = localStorage.getItem("devforge_session");
  if (token) {
    headers["Authorization"] = `Bearer ${JSON.parse(token).token}`;
  }

  try {
    const response = await fetch(url, { ...options, headers });

    if (response.status === 401) {
      localStorage.removeItem("devforge_session");
      return { data: null, error: "Unauthorized", status: 401 };
    }

    if (response.status === 429) {
      return { data: null, error: "Rate limited", status: 429 };
    }

    const body = await response.json();

    if (response.status >= 400 && response.status < 500) {
      return {
        data: null,
        error: body.message || `Client error (${response.status})`,
        status: response.status,
      };
    }

    if (response.status >= 500) {
      return {
        data: null,
        error: body.message || `Server error (${response.status})`,
        status: response.status,
      };
    }

    if (!validateApiResponse(body)) {
      return { data: null, error: "Invalid response format", status: 502 };
    }

    return { data: body as T, error: null, status: response.status };
  } catch (err) {
    return {
      data: null,
      error: err instanceof Error ? err.message : "Network error",
      status: 0,
    };
  }
}

async function fetchApi<T>(endpoint: string): Promise<ApiResponse<T>> {
  return request<T>(endpoint, { method: "GET" });
}

async function postApi<T>(endpoint: string, body: unknown): Promise<ApiResponse<T>> {
  return request<T>(endpoint, {
    method: "POST",
    body: JSON.stringify(body),
  });
}

async function deleteApi<T>(endpoint: string): Promise<ApiResponse<T>> {
  return request<T>(endpoint, { method: "DELETE" });
}

async function fetchProjects<T>(userId: string): Promise<ApiResponse<T>> {
  return fetchApi<T>(`/projects?user=${userId}`);
}

async function fetchArtifacts<T>(projectId: string): Promise<ApiResponse<T>> {
  return fetchApi<T>(`/artifacts?project=${projectId}`);
}

export { fetchApi, postApi, deleteApi, fetchProjects, fetchArtifacts };
export type { ApiResponse, Project, Artifact, User };
