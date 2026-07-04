import { browser } from "wxt/browser";
import { defineBackground } from "wxt/utils/define-background";

export default defineBackground(() => {
  // The side panel is our only UI entrypoint. Opening it from the toolbar click
  // keeps the UX simple and also makes debugging easier because there is a
  // single place where the extension bootstraps.
  if (!browser.sidePanel?.setPanelBehavior) {
    return;
  }

  void browser.sidePanel.setPanelBehavior({
    openPanelOnActionClick: true,
  });
});
