import { describe, expect, it } from "vitest";
import {
  buildRemoveDeviceContext,
  clearSelectedSessionContext,
} from "./deviceSelection";

const devices = [
  {
    id: "office",
    name: "Office",
    origin: "https://office.local",
    input: "office.local",
    createdAt: "2026-01-01T00:00:00.000Z",
  },
  {
    id: "lab",
    name: "Lab",
    origin: "https://lab.local",
    input: "lab.local",
    createdAt: "2026-01-01T00:00:00.000Z",
  },
];

describe("device selection helpers", () => {
  it("builds the next context after removing the selected device", () => {
    expect(
      buildRemoveDeviceContext({
        devices,
        sessions: {
          office: {
            accessToken: "token",
            username: "admin",
            admin: true,
            signedInAt: "2026-01-01T00:00:00.000Z",
          },
        },
        selectedDeviceId: "office",
        deviceId: "office",
      }),
    ).toMatchObject({
      hadSession: true,
      selectedDeviceId: "lab",
      devices: [devices[1]],
    });
  });

  it("clears the selected device session only", () => {
    expect(
      clearSelectedSessionContext(
        {
          office: {
            accessToken: "token",
            username: "admin",
            admin: true,
            signedInAt: "2026-01-01T00:00:00.000Z",
          },
          lab: {
            accessToken: "token-2",
            username: "user",
            admin: false,
            signedInAt: "2026-01-01T00:00:00.000Z",
          },
        },
        "office",
      ),
    ).toEqual({
      lab: {
        accessToken: "token-2",
        username: "user",
        admin: false,
        signedInAt: "2026-01-01T00:00:00.000Z",
      },
    });
  });
});
