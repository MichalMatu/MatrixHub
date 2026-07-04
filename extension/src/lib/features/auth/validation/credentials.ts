import { createAppError } from "$lib/domain/shared/appError";
import { err, ok, type Result } from "$lib/domain/shared/result";

export interface CredentialsValues {
  username: string;
  password: string;
}

export interface CredentialErrors {
  username?: string;
  password?: string;
}

export function createCredentialErrors(): CredentialErrors {
  return {};
}

export function validateCredentials(
  values: CredentialsValues,
): Result<CredentialsValues, CredentialErrors> {
  const normalized = {
    username: values.username.trim(),
    password: values.password,
  };

  const errors: CredentialErrors = {};

  if (!normalized.username) {
    errors.username = createAppError("validation/username_required").message;
  }

  if (!normalized.password) {
    errors.password = createAppError("validation/password_required").message;
  }

  if (errors.username || errors.password) {
    return err(errors);
  }

  return ok(normalized);
}
