import {
  decodeAccessTokenPayload,
  signInToDevice,
  type DeviceRecord,
  type DeviceSession,
} from "@matrixhub/device-sdk";
import { createAppError } from "$lib/domain/shared/appError";
import { ensureDeviceOriginPermission } from "$lib/infrastructure/chrome/permissions";
import {
  validateCredentials,
  type CredentialErrors,
} from "$lib/features/auth/validation/credentials";

interface SignInDeviceInput {
  selectedDevice: DeviceRecord;
  username: string;
  password: string;
}

type SignInDeviceResult =
  | {
      kind: "success";
      session: DeviceSession;
    }
  | {
      kind: "validation_error";
      errors: CredentialErrors;
      message: string;
    }
  | {
      kind: "permission_denied";
      message: string;
    }
  | {
      kind: "invalid_credentials";
      message: string;
    };

export async function signInSelectedDevice(
  input: SignInDeviceInput,
): Promise<SignInDeviceResult> {
  const validation = validateCredentials({
    username: input.username,
    password: input.password,
  });

  if (!validation.ok) {
    return {
      kind: "validation_error",
      errors: validation.error,
      message:
        validation.error.username ??
        validation.error.password ??
        createAppError("request/failed").message,
    };
  }

  const allowed = await ensureDeviceOriginPermission(
    input.selectedDevice.origin,
  );
  if (!allowed) {
    return {
      kind: "permission_denied",
      message: createAppError("permissions/host_denied").message,
    };
  }

  const result = await signInToDevice({
    baseUrl: input.selectedDevice.origin,
    credentials: {
      username: validation.value.username,
      password: validation.value.password,
    },
  });

  if (!result) {
    return {
      kind: "invalid_credentials",
      message: createAppError("auth/invalid_credentials").message,
    };
  }

  const payload = decodeAccessTokenPayload(result.access_token);

  return {
    kind: "success",
    session: {
      accessToken: result.access_token,
      username: payload?.username ?? validation.value.username,
      admin: !!payload?.admin,
      signedInAt: new Date().toISOString(),
    },
  };
}
