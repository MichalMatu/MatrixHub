import { resolve } from "node:path";
import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    environment: "node",
    globals: true,
    include: ["src/**/*.{test,spec}.ts"],
    coverage: {
      provider: "v8",
      include: [
        "src/lib/domain/shared/appError.ts",
        "src/lib/infrastructure/chrome/storage.ts",
        "src/lib/infrastructure/chrome/cookies.ts",
        "src/lib/infrastructure/chrome/permissions.ts",
        "src/lib/features/auth/actions/signIn.ts",
        "src/lib/features/auth/actions/sessionFlows.ts",
        "src/lib/features/devices/actions/deviceDraft.ts",
        "src/lib/features/devices/actions/deviceSelection.ts",
        "src/lib/features/overview/mappers/viewModels.ts",
        "src/lib/features/realtime/socket/deviceOverviewSocket.ts",
        "src/lib/features/sidepanel/controller/sidepanelController.ts",
      ],
      thresholds: {
        statements: 65,
        branches: 55,
        functions: 75,
        lines: 65,
      },
    },
  },
  resolve: {
    alias: {
      "@matrixhub/device-sdk": resolve(
        __dirname,
        "../packages/device-sdk/src/index.ts",
      ),
      $lib: resolve(__dirname, "./src/lib"),
    },
  },
});
