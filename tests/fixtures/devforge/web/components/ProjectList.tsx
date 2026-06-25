/** Project list component with filtering, sorting, and selection. */

import type { Project } from "../services/api";
import { validateProjectId } from "../utils/validation";

type SortField = "name" | "updatedAt" | "artifactCount";
type SortDirection = "asc" | "desc";

interface FilterOptions<T> {
  visibility?: string;
  search?: string;
  ownerId?: string;
}

interface ProjectListProps {
  projects: Project[];
  selectedId: string | null;
  sortField?: SortField;
  sortDirection?: SortDirection;
  onSelect: (id: string) => void;
}

function filterProjects<T extends Project>(
  projects: T[],
  options: FilterOptions<T>
): T[] {
  return projects.filter((p) => {
    if (options.visibility && p.visibility !== options.visibility) {
      return false;
    }
    if (options.ownerId && p.ownerId !== options.ownerId) {
      return false;
    }
    if (
      options.search &&
      !p.name.toLowerCase().includes(options.search.toLowerCase())
    ) {
      return false;
    }
    return true;
  });
}

function sortProjects(
  projects: Project[],
  field: SortField,
  direction: SortDirection
): Project[] {
  const sorted = [...projects];
  const multiplier = direction === "asc" ? 1 : -1;

  sorted.sort((a, b) => {
    let comparison = 0;
    if (field === "name") {
      comparison = a.name.localeCompare(b.name);
    } else if (field === "updatedAt") {
      comparison = a.updatedAt.getTime() - b.updatedAt.getTime();
    } else if (field === "artifactCount") {
      comparison = a.artifactCount - b.artifactCount;
    }
    return comparison * multiplier;
  });

  return sorted;
}

function groupByVisibility(projects: Project[]): Map<string, Project[]> {
  const groups = new Map<string, Project[]>();
  for (const project of projects) {
    const key = project.visibility;
    if (!groups.has(key)) {
      groups.set(key, []);
    }
    groups.get(key)!.push(project);
  }
  return groups;
}

export function createProjectList(
  container: HTMLElement,
  props: ProjectListProps
): void {
  const {
    projects,
    selectedId,
    sortField = "name",
    sortDirection = "asc",
    onSelect,
  } = props;

  const sorted = sortProjects(projects, sortField, sortDirection);
  const grouped = groupByVisibility(sorted);

  container.innerHTML = "";
  for (const [visibility, groupProjects] of grouped) {
    const header = document.createElement("h3");
    header.textContent = visibility.toUpperCase();
    container.appendChild(header);

    for (const project of groupProjects) {
      if (!validateProjectId(project.id)) {
        continue;
      }

      const item = document.createElement("div");
      item.className =
        "project-item" + (project.id === selectedId ? " selected" : "");
      item.textContent = project.name;
      item.dataset.projectId = project.id;
      item.addEventListener("click", () => onSelect(project.id));
      container.appendChild(item);
    }
  }
}

export { filterProjects, sortProjects, groupByVisibility };
export type { FilterOptions, ProjectListProps, SortField, SortDirection };
