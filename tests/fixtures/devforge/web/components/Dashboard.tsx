/** Dashboard component showing project overview and quick actions. */

import { useAuth, useWebSocket } from "../hooks";
import { fetchProjects, type Project } from "../services/api";
import { formatDate, formatBytes } from "../utils/format";

interface DashboardProps {
  userId: string;
  refreshInterval?: number;
}

interface DashboardState {
  projects: Project[];
  loading: boolean;
  error: string | null;
  lastUpdated: Date | null;
}

async function loadProjects(userId: string): Promise<Project[]> {
  const { data, error } = await fetchProjects<Project[]>(userId);
  if (error) {
    throw new Error(`Failed to load projects: ${error}`);
  }
  return data ?? [];
}

function renderProjectCard(project: Project): string {
  const fields = [
    `Name: ${project.name}`,
    `Visibility: ${project.visibility}`,
    `Artifacts: ${project.artifactCount}`,
    `Size: ${formatBytes(project.totalSize)}`,
    `Updated: ${formatDate(project.updatedAt)}`,
  ];
  return fields.join("\n");
}

async function refreshDashboard(
  userId: string,
  onUpdate: (projects: Project[]) => void
): Promise<void> {
  const projects = await loadProjects(userId);
  onUpdate(projects);
}

export function createDashboard(
  container: HTMLElement,
  props: DashboardProps
): () => void {
  const { userId, refreshInterval = 30000 } = props;

  let state: DashboardState = {
    projects: [],
    loading: true,
    error: null,
    lastUpdated: null,
  };

  const render = () => {
    container.innerHTML = "";
    if (state.loading) {
      container.textContent = "Loading dashboard...";
      return;
    }
    if (state.error) {
      container.textContent = `Error: ${state.error}`;
      return;
    }
    for (const project of state.projects) {
      const card = document.createElement("div");
      card.className = "project-card";
      card.textContent = renderProjectCard(project);
      container.appendChild(card);
    }
  };

  const updateProjects = (projects: Project[]) => {
    state = { ...state, projects, loading: false, lastUpdated: new Date() };
    render();
  };

  const handleError = (error: Error) => {
    state = { ...state, loading: false, error: error.message };
    render();
  };

  refreshDashboard(userId, updateProjects).catch(handleError);
  const interval = setInterval(() => {
    refreshDashboard(userId, updateProjects).catch(console.error);
  }, refreshInterval);

  render();

  return () => {
    clearInterval(interval);
  };
}

export type { DashboardProps, DashboardState };
