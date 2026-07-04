import type { DeviceRecord } from "./types";

function stripTrailingSlashes(value: string) {
  return value.replace(/\/+$/, "");
}

export function normalizeDeviceOrigin(input: string, defaultProtocol = "https:") {
  const trimmed = input.trim();
  if (!trimmed) {
    throw new Error("Device address is required.");
  }

  const candidate = /^[a-z][a-z0-9+.-]*:\/\//i.test(trimmed)
    ? trimmed
    : `${defaultProtocol}//${trimmed}`;

  const url = new URL(candidate);
  if (!["http:", "https:"].includes(url.protocol)) {
    throw new Error("Only http:// and https:// device addresses are supported.");
  }

  url.pathname = "";
  url.search = "";
  url.hash = "";

  return {
    origin: stripTrailingSlashes(url.origin),
    host: url.hostname,
    protocol: url.protocol,
    port: url.port
  };
}

export function buildDevicePermissionOrigin(origin: string) {
  return `${stripTrailingSlashes(origin)}/*`;
}

export function suggestDeviceName(origin: string) {
  const { host } = normalizeDeviceOrigin(origin);
  return host.replace(/\.local$/i, "") || host;
}

export function deriveDeviceId(origin: string) {
  return normalizeDeviceOrigin(origin).origin.replace(/[^a-z0-9]+/gi, "-").replace(/^-|-$/g, "").toLowerCase();
}

export function createDeviceRecord(input: {
  origin: string;
  rawInput: string;
  name?: string;
  existing?: DeviceRecord | null;
}): DeviceRecord {
  const normalizedOrigin = normalizeDeviceOrigin(input.origin).origin;
  const now = new Date().toISOString();

  return {
    id: input.existing?.id ?? deriveDeviceId(normalizedOrigin),
    name: (input.name?.trim() || input.existing?.name || suggestDeviceName(normalizedOrigin)).trim(),
    origin: normalizedOrigin,
    input: input.rawInput.trim(),
    createdAt: input.existing?.createdAt ?? now,
    lastConnectedAt: input.existing?.lastConnectedAt
  };
}
