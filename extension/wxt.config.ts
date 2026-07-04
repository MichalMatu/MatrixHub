import { resolve } from "node:path";
import { defineConfig } from "wxt";
import Icons from "unplugin-icons/vite";

export default defineConfig({
  modules: ["@wxt-dev/module-svelte"],
  manifest: {
    default_locale: "en",
    name: "__MSG_extensionName__",
    description: "__MSG_extensionDescription__",
    icons: {
      16: "icon-16.png",
      32: "icon-32.png",
      48: "icon-48.png",
      128: "icon-128.png",
    },
    // storage: persist known devices and cached sessions
    // cookies: mirror the access token to /ws because the ESP32 WebSocket auth
    // currently reads a cookie instead of an Authorization header
    // sidePanel: main UX surface of the extension
    permissions: ["storage", "cookies", "sidePanel"],
    // Device IPs and mDNS hostnames are only known at runtime, so we request
    // origin access lazily after the user adds a device.
    optional_host_permissions: ["http://*/*", "https://*/*"],
    action: {
      default_title: "__MSG_extensionActionTitle__",
      default_icon: {
        16: "icon-16.png",
        32: "icon-32.png",
        48: "icon-48.png",
      },
    },
  },
  vite: () => ({
    plugins: [
      Icons({
        compiler: "svelte",
      }),
    ],
    resolve: {
      alias: {
        // The extension and the main interface share one SDK so API shapes,
        // auth helpers and WS parsers stay consistent across both clients.
        "@matrixhub/device-sdk": resolve(
          __dirname,
          "../packages/device-sdk/src/index.ts",
        ),
        $lib: resolve(__dirname, "./src/lib"),
      },
    },
  }),
});
