# MatrixHub Frontend

This directory contains the SvelteKit frontend for the MatrixHub dashboard application.

## Overview

The frontend is built with modern web technologies to provide a responsive, accessible, and performant user interface for IoT device management.

## Tech Stack

- **SvelteKit 2**: Full-stack framework with SSR/SSG support
- **Svelte 5**: With runes for reactive state management
- **TypeScript 5**: Type-safe development
- **TailwindCSS 4**: Utility-first styling
- **DaisyUI**: Component library for consistent UI
- **Vite 7**: Fast build tool with HMR
- **Vitest**: Unit testing
- **Playwright**: End-to-end testing

## Getting Started

### Prerequisites

- Node.js 18+
- npm or yarn

### Installation

```bash
npm install
```

### Development

```bash
npm run dev
```

Starts the development server at http://localhost:5173 with hot reload.

### Building

```bash
npm run build
```

Creates a production build in `build/` directory. In the default project configuration, PlatformIO then converts that output into an embedded PROGMEM header via `EMBED_WWW`.

### Testing

```bash
npm run test:run        # Unit tests
npm run test:e2e        # E2E tests
npm run test:coverage   # Coverage report
```

### Linting

```bash
npm run lint            # Check and fix formatting/linting
```

## Project Structure

```
src/
├── lib/
│   ├── components/      # Reusable UI components
│   │   ├── common/     # Basic components (Spinner, Alert)
│   │   ├── forms/      # Form components (Input, Button)
│   │   ├── layout/     # Layout components (Card, Grid)
│   │   └── ...
│   ├── services/        # API services and utilities
│   │   ├── api/        # REST API clients
│   │   └── core/       # Core services (Logger, Auth)
│   ├── stores/          # Svelte stores for state management
│   ├── utils/           # Utility functions
│   └── i18n/            # Internationalization
├── routes/              # SvelteKit pages and layouts
│   ├── (dashboard)/    # Dashboard pages
│   ├── connections/    # Connection settings
│   ├── settings/       # App settings
│   └── ...
└── app.html             # Root HTML template
```

## Key Features

### State Management

- **Svelte runes**: For local component state
- **Svelte stores**: For global app state (user, connection, etc.)
- **Reactive composables**: Custom hooks like `usePolling` for lifecycle management

### API Integration

- Centralized API clients with error handling
- Request cancellation and timeout support
- JWT authentication with backend-driven session validation

### UI/UX

- Responsive design (mobile-first)
- Dark/light theme support via DaisyUI
- Toast notifications for user feedback
- Loading states and error boundaries

### Development Tools

- Hot module replacement (HMR)
- TypeScript for type safety
- ESLint + Prettier for code quality
- Visualizer for bundle analysis

## API Usage

### Authentication

```typescript
import { signIn } from '$lib/services/api/core/SecurityApiService';

const response = await signIn({ username: 'admin', password: 'admin' });
```

### Polling Hook

```typescript
import { usePolling } from '$lib/utils/api/usePolling.svelte';

usePolling(fetchData, {
	intervalMs: 5000,
	pauseWhenHidden: true,
	jitter: true
});
```

### Component Example

```svelte
<script lang="ts">
	import { BaseCard } from '$lib/components';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
</script>

<BaseCard title={m.dashboard_title({ locale: i18n.languageTag })}>
	<!-- Content -->
</BaseCard>
```

## Deployment

The default deployment mode is embedded web assets in firmware, not a separate filesystem image. The build process is:

1. `npm run build` creates optimized assets
2. `scripts/build/build_interface.py` converts assets to `lib/framework/core/WWWData.h`
3. PlatformIO compiles that header into the firmware when `EMBED_WWW` is enabled

Running `pio run -e waveshare_esp32s3_matrix` is the normal end-to-end flow. A LittleFS-hosted web UI is only a fallback mode for custom configurations where `EMBED_WWW` is disabled.

For development, use `npm run dev` with proxy to ESP32 device.

## Contributing

- Follow TypeScript and Svelte best practices
- Add JSDoc comments for public APIs
- Write tests for new features
- Update this README for significant changes

## Troubleshooting

- **Build fails**: Clear `node_modules` and reinstall
- **HMR not working**: Check Vite config and proxy settings
- **API errors**: Verify ESP32 device is running and accessible
- **Styling issues**: Check TailwindCSS config and DaisyUI classes
