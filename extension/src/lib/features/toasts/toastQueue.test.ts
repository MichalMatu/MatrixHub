import { afterEach, describe, expect, it, vi } from "vitest";
import { createToastQueue } from "./toastQueue";

describe("createToastQueue", () => {
  afterEach(() => {
    vi.useRealTimers();
  });

  it("deduplicates identical toasts and auto-dismisses them", () => {
    vi.useFakeTimers();

    let items: Array<{ id: number; tone: "info" | "error"; message: string }> =
      [];
    const queue = createToastQueue({
      lifetimeMs: 1000,
      onChange: (next) => {
        items = next;
      },
    });

    queue.show("info", "Saved");
    queue.show("info", "Saved");

    expect(items).toHaveLength(1);
    expect(items[0]?.message).toBe("Saved");

    vi.advanceTimersByTime(1000);

    expect(items).toEqual([]);
  });

  it("keeps only the newest entries up to maxToasts", () => {
    let items: Array<{ id: number; tone: "info" | "error"; message: string }> =
      [];
    const queue = createToastQueue({
      maxToasts: 2,
      onChange: (next) => {
        items = next;
      },
    });

    queue.show("info", "One");
    queue.show("error", "Two");
    queue.show("info", "Three");

    expect(items.map((item) => item.message)).toEqual(["Two", "Three"]);
  });

  it("updates a keyed toast in place", () => {
    vi.useFakeTimers();

    let items: Array<{
      id: number;
      tone: "info" | "error";
      message: string;
      key?: string;
    }> = [];
    const queue = createToastQueue({
      lifetimeMs: 1000,
      onChange: (next) => {
        items = next;
      },
    });

    queue.show("info", "Saving LED...", { key: "matrix-led" });
    queue.show("info", "LED saved.", { key: "matrix-led" });

    expect(items).toHaveLength(1);
    expect(items[0]?.message).toBe("LED saved.");

    vi.advanceTimersByTime(1000);

    expect(items).toEqual([]);
  });

  it("dismisses a keyed toast", () => {
    let items: Array<{
      id: number;
      tone: "info" | "error";
      message: string;
      key?: string;
    }> = [];
    const queue = createToastQueue({
      onChange: (next) => {
        items = next;
      },
    });

    queue.show("info", "Saving LED...", { key: "matrix-led" });
    queue.dismissByKey("matrix-led");

    expect(items).toEqual([]);
  });
});
