import { describe, expect, it, vi } from "vitest";
import { createInitialSidepanelState } from "../state/sidepanelState";
import { createSidepanelControllerStore } from "./sidepanelControllerStore";

describe("sidepanel controller store", () => {
  it("emits named transition metadata when a commit updates state", async () => {
    const transitions: Array<{
      label: string;
      persist: string;
      selectedDeviceId: string | null;
      activeScreen: string;
      realtimeState: string;
    }> = [];
    const onStateChange = vi.fn();
    const saveExtensionState = vi.fn(async () => undefined);

    const store = createSidepanelControllerStore({
      initialState: createInitialSidepanelState(),
      onStateChange,
      onError: vi.fn(),
      saveExtensionState,
      onTransition: (transition) => {
        transitions.push(transition);
      },
    });

    await store.commit(
      "selection.changed",
      (state) => {
        state.selectedDeviceId = "office";
      },
      {
        persist: "await",
      },
    );

    expect(onStateChange).toHaveBeenCalledTimes(1);
    expect(saveExtensionState).toHaveBeenCalledTimes(1);
    expect(transitions).toEqual([
      {
        label: "selection.changed",
        persist: "await",
        selectedDeviceId: "office",
        activeScreen: "devices",
        realtimeState: "idle",
      },
    ]);
  });

  it("routes background persistence failures to onError", async () => {
    const saveError = new Error("save failed");
    const onError = vi.fn();

    const store = createSidepanelControllerStore({
      initialState: createInitialSidepanelState(),
      onStateChange: vi.fn(),
      onError,
      saveExtensionState: vi.fn(async () => {
        throw saveError;
      }),
    });

    await store.commit(
      "ui.sectionVisibility.changed",
      (state) => {
        state.sectionVisibility.bluetooth = false;
      },
      {
        persist: "background",
      },
    );

    await vi.waitFor(() => {
      expect(onError).toHaveBeenCalledWith(saveError);
    });
  });
});
