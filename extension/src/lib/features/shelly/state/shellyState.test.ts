import { describe, expect, it } from "vitest";
import {
  applyShellyEvent,
  applyShellyOptimisticState,
  applyShellySnapshot,
  resolveShellyEventDeviceId,
} from "./shellyState";

const devices = [
  {
    id: "relay-alpha-01",
    name: "Desk Lamp",
    isOn: false,
    isOnline: true,
  },
  {
    id: "relay-beta-02",
    name: "Pump",
    isOn: true,
    isOnline: true,
  },
];

describe("shelly state helpers", () => {
  it("sorts Shelly snapshots by display name", () => {
    expect(applyShellySnapshot([...devices].reverse())).toEqual(devices);
  });

  it("matches binary events against a unique id prefix", () => {
    expect(
      applyShellyEvent(devices, {
        id: "relay-al",
        isOn: true,
        isOnline: true,
      }),
    ).toEqual([
      {
        id: "relay-alpha-01",
        name: "Desk Lamp",
        isOn: true,
        isOnline: true,
      },
      devices[1],
    ]);
  });

  it("does not resolve an ambiguous id prefix", () => {
    expect(
      resolveShellyEventDeviceId(
        [
          ...devices,
          {
            id: "relay-alpha-02",
            name: "Heater",
            isOn: false,
            isOnline: true,
          },
        ],
        "relay-alpha",
      ),
    ).toBeNull();
  });

  it("updates relay state optimistically by exact id", () => {
    expect(applyShellyOptimisticState(devices, "relay-beta-02", false)).toEqual(
      [
        devices[0],
        {
          id: "relay-beta-02",
          name: "Pump",
          isOn: false,
          isOnline: true,
        },
      ],
    );
  });
});
