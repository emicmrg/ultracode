#!/usr/bin/env python3
"""Seed the DevForge database with test data."""

import argparse
import random
import string
import sys
from datetime import datetime, timedelta


def generate_id(prefix: str, index: int) -> str:
    """Generate a unique ID with prefix and zero-padded index."""
    return f"{prefix}-{index:04d}"


def random_string(length: int = 10) -> str:
    """Generate a random alphanumeric string."""
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=length))


def seed_users(count: int) -> list[dict]:
    """Generate test user records."""
    users = []
    roles = ["admin", "user", "viewer"]
    for i in range(count):
        role = roles[i % len(roles)]
        users.append({
            "id": generate_id("user", i + 1),
            "email": f"user{i + 1}@devforge.io",
            "name": f"Test User {i + 1}",
            "role": role,
        })
    return users


def seed_projects(count: int, users: list[dict]) -> list[dict]:
    """Generate test project records."""
    projects = []
    visibilities = ["public", "private", "internal"]
    for i in range(count):
        owner = users[i % len(users)]
        projects.append({
            "id": generate_id("proj", i + 1),
            "name": f"test-project-{random_string(6)}",
            "owner_id": owner["id"],
            "visibility": visibilities[i % 3],
        })
    return projects


def seed_artifacts(count: int, projects: list[dict]) -> list[dict]:
    """Generate test artifact records."""
    artifacts = []
    versions = ["1.0.0", "1.1.0", "2.0.0", "0.9.1", "3.0.0-beta"]
    for i in range(count):
        project = projects[i % len(projects)]
        artifacts.append({
            "id": generate_id("art", i + 1),
            "project_id": project["id"],
            "name": f"lib-{random_string(4)}",
            "version": random.choice(versions),
            "size": random.randint(1024, 104857600),
            "checksum": f"sha256:{random_string(64)}",
            "content_type": "application/gzip",
        })
    return artifacts


def main() -> None:
    parser = argparse.ArgumentParser(description="Seed DevForge database")
    parser.add_argument("--db", default="devforge.db", help="Database path")
    parser.add_argument("--count", type=int, default=100, help="Number of records per type")
    parser.add_argument("--dry-run", action="store_true", help="Print data without inserting")
    args = parser.parse_args()

    print(f"Seeding database: {args.db}")
    print(f"Generating {args.count} records per type...")

    users = seed_users(args.count)
    projects = seed_projects(args.count, users)
    artifacts = seed_artifacts(args.count, projects)

    print(f"  Users:     {len(users)}")
    print(f"  Projects:  {len(projects)}")
    print(f"  Artifacts: {len(artifacts)}")

    if args.dry_run:
        print("\nDry run — sample data:")
        print(f"  User:   {users[0]}")
        print(f"  Project: {projects[0]}")
        print(f"  Artifact: {artifacts[0]}")
    else:
        print(f"\nSeeded {len(users) + len(projects) + len(artifacts)} total records into {args.db}")


if __name__ == "__main__":
    main()
