/** Validation utilities for forms, IDs, checksums, and API responses. */

const ID_PATTERN = /^[a-zA-Z0-9][a-zA-Z0-9_.-]{0,127}$/;
const SEMVER_PATTERN = /^\d+\.\d+\.\d+(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$/;
const CHECKSUM_PATTERN = /^sha256:[a-f0-9]{64}$/i;
const EMAIL_PATTERN = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
const URL_PATTERN = /^https?:\/\/[^\s/$.?#].[^\s]*$/i;

function validateProjectId(id: string): boolean {
  if (!id || typeof id !== "string") {
    return false;
  }
  return ID_PATTERN.test(id);
}

function validateUserId(id: string): boolean {
  return validateProjectId(id);
}

function validateArtifactName(name: string): boolean {
  if (!name || typeof name !== "string") {
    return false;
  }
  if (name.length > 256) {
    return false;
  }
  return ID_PATTERN.test(name);
}

function validateVersion(version: string): boolean {
  if (!version || typeof version !== "string") {
    return false;
  }
  return SEMVER_PATTERN.test(version);
}

function validateChecksum(checksum: string): boolean {
  if (!checksum || typeof checksum !== "string") {
    return false;
  }
  return CHECKSUM_PATTERN.test(checksum);
}

function validateEmail(email: string): boolean {
  if (!email || typeof email !== "string") {
    return false;
  }
  return EMAIL_PATTERN.test(email);
}

function validateUrl(url: string): boolean {
  if (!url || typeof url !== "string") {
    return false;
  }
  return URL_PATTERN.test(url);
}

function validatePassword(password: string): { valid: boolean; reason?: string } {
  if (!password || typeof password !== "string") {
    return { valid: false, reason: "Password is required" };
  }
  if (password.length < 8) {
    return { valid: false, reason: "Password must be at least 8 characters" };
  }
  if (password.length > 128) {
    return { valid: false, reason: "Password must be at most 128 characters" };
  }
  if (!/[A-Z]/.test(password)) {
    return { valid: false, reason: "Password must contain an uppercase letter" };
  }
  if (!/[a-z]/.test(password)) {
    return { valid: false, reason: "Password must contain a lowercase letter" };
  }
  if (!/[0-9]/.test(password)) {
    return { valid: false, reason: "Password must contain a digit" };
  }
  return { valid: true };
}

function validateApiResponse(response: unknown): boolean {
  if (!response || typeof response !== "object") {
    return false;
  }
  const obj = response as Record<string, unknown>;
  return "status" in obj || "data" in obj || Array.isArray(response);
}

function sanitizeInput(input: string): string {
  return input
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#x27;");
}

export {
  validateProjectId,
  validateUserId,
  validateArtifactName,
  validateVersion,
  validateChecksum,
  validateEmail,
  validateUrl,
  validatePassword,
  validateApiResponse,
  sanitizeInput,
  ID_PATTERN,
  SEMVER_PATTERN,
  CHECKSUM_PATTERN,
  EMAIL_PATTERN,
  URL_PATTERN,
};
