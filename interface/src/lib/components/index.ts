/**
 * Components barrel export
 *
 * This file is the single public entrypoint for shared UI primitives.
 *
 * Import components using:
 *   - import { Spinner } from '$lib/components'
 *   - import { Spinner } from '$lib/components/common'
 *
 * Organized into categories:
 * - common/ - General-purpose UI components (Spinner)
 * - forms/ - Dialog flows (ConfirmDialog, InfoDialog, PromptDialog)
 * - toasts/ - Toast notification components
 */

// Common components
export { Spinner } from './common';
export { default as Modal } from './Modal.svelte';

// Form components
export { default as ConfirmDialog } from './forms/ConfirmDialog.svelte';
export { default as InfoDialog } from './forms/InfoDialog.svelte';
export { default as RestartProgressDialog } from './forms/RestartProgressDialog.svelte';
export { default as PromptDialog } from './forms/PromptDialog.svelte';

// Note: toasts/ has its own internal exports
// Import them directly: import { ... } from '$lib/components/toasts'
