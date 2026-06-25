#!/usr/bin/env python3
"""Generate TypeScript type definitions from Python models."""

import os
from dataclasses import dataclass, fields
from typing import Any


@dataclass
class ModelField:
    name: str
    type: str
    optional: bool = False
    description: str = ""


MODELS: dict[str, list[ModelField]] = {
    "User": [
        ModelField("id", "string", description="Unique user identifier"),
        ModelField("email", "string", description="User email address"),
        ModelField("name", "string", description="Display name"),
        ModelField("role", "'admin' | 'user' | 'viewer'", description="User role"),
    ],
    "Project": [
        ModelField("id", "string", description="Unique project identifier"),
        ModelField("name", "string", description="Project name"),
        ModelField("ownerId", "string", description="Owner user ID"),
        ModelField("visibility", "'public' | 'private' | 'internal'", description="Visibility level"),
        ModelField("settings", "ProjectSettings", description="Project configuration"),
        ModelField("tags", "string[]", optional=True, description="Project tags"),
    ],
    "ProjectSettings": [
        ModelField("maxArtifacts", "number", description="Max artifacts to retain"),
        ModelField("retentionDays", "number", description="Days to retain artifacts"),
        ModelField("defaultBranch", "string", description="Default VCS branch"),
        ModelField("enableCi", "boolean", description="Enable CI/CD"),
    ],
    "Artifact": [
        ModelField("id", "string", description="Unique artifact identifier"),
        ModelField("projectId", "string", description="Parent project ID"),
        ModelField("name", "string", description="Artifact name"),
        ModelField("version", "string", description="Semantic version"),
        ModelField("size", "number", description="Size in bytes"),
        ModelField("checksum", "string", description="SHA-256 checksum"),
        ModelField("contentType", "string", description="MIME type"),
        ModelField("createdAt", "string", description="ISO 8601 timestamp"),
    ],
    "ApiResponse<T>": [
        ModelField("data", "T | null", description="Response payload"),
        ModelField("error", "string | null", description="Error message"),
        ModelField("status", "number", description="HTTP status code"),
    ],
}


def render_field(field: ModelField) -> str:
    """Render a single field as a TypeScript property."""
    optional = "?" if field.optional else ""
    return f"  /** {field.description} */\n  {field.name}{optional}: {field.type};"


def render_model(name: str, fields: list[ModelField]) -> str:
    """Render a complete TypeScript interface."""
    rendered_fields = "\n".join(render_field(f) for f in fields)
    return f"export interface {name} {{\n{rendered_fields}\n}}\n"


def main() -> None:
    output_dir = os.path.join(
        os.path.dirname(__file__), "..", "web", "types"
    )
    os.makedirs(output_dir, exist_ok=True)

    output_path = os.path.join(output_dir, "models.ts")
    with open(output_path, "w") as f:
        f.write("// Auto-generated TypeScript types from Python models\n")
        f.write("// Do not edit manually. Run: python scripts/gen_types.py\n\n")
        for name, model_fields in MODELS.items():
            f.write(render_model(name, model_fields))
            f.write("\n")

    print(f"Generated {len(MODELS)} types to {output_path}")


if __name__ == "__main__":
    main()
