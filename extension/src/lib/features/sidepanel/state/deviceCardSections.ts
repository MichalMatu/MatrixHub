export type DeviceCardSectionKey = "matrix" | "bluetooth" | "shelly";

export interface DeviceCardSectionState {
  matrix: boolean;
  bluetooth: boolean;
  shelly: boolean;
}

const DEFAULT_DEVICE_CARD_SECTION_STATE: DeviceCardSectionState = {
  matrix: true,
  bluetooth: true,
  shelly: true,
};

export function createDefaultDeviceCardSectionState(): DeviceCardSectionState {
  return {
    ...DEFAULT_DEVICE_CARD_SECTION_STATE,
  };
}

export function sanitizeDeviceCardSectionState(
  raw: unknown,
): DeviceCardSectionState {
  const defaults = createDefaultDeviceCardSectionState();

  if (!raw || typeof raw !== "object") {
    return defaults;
  }

  const value = raw as Record<string, unknown>;

  return {
    matrix: typeof value.matrix === "boolean" ? value.matrix : defaults.matrix,
    bluetooth:
      typeof value.bluetooth === "boolean"
        ? value.bluetooth
        : defaults.bluetooth,
    shelly: typeof value.shelly === "boolean" ? value.shelly : defaults.shelly,
  };
}
