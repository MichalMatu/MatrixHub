import { describe, expect, it } from "vitest";

import { parseMatrixSettings } from "./matrix";

function validMatrixSettings(brightness: number) {
  return {
    brightness,
    alarm_mode: 1,
    rotation: 0,
    auto_rotate: false,
    effect_enabled: false,
    effect_engine: 0,
    effect_mode: 0,
    effect_speed: 1000,
    effect_color: 0x00ff00,
    effect_color_2: 0xff0000,
    effect_color_3: 0x0000ff,
    effect_reactivity_provider: 0,
    effect_reactivity_gain: 80,
    menu_enabled: true,
    menu_text_color: 0xffffff,
    menu_scroll_speed: 20,
  };
}

describe("parseMatrixSettings", () => {
  it.each([
    [0, 2],
    [1, 2],
    [2, 2],
    [255, 255],
  ])("normalizes persisted brightness %i to %i", (input, expected) => {
    expect(parseMatrixSettings(validMatrixSettings(input))?.brightness).toBe(
      expected,
    );
  });
});
