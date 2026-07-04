import { mount } from "svelte";
import "../../src/app.css";
import App from "./App.svelte";
import { applyStoredSidepanelTheme } from "../../src/lib/features/theme/themePresets";

applyStoredSidepanelTheme();

mount(App, {
  target: document.getElementById("app")!,
});
