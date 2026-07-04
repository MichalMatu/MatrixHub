import { describe, expect, it } from 'vitest';
import { UnsavedChangesRegistry } from './unsavedChanges.svelte';

describe('UnsavedChangesRegistry', () => {
	it('tracks dirty sources independently', () => {
		const registry = new UnsavedChangesRegistry();

		registry.setSourceDirty('matrix-display', true);
		registry.setSourceDirty('matrix-effects', false);

		expect(registry.hasChanges).toBe(true);
		expect(registry.dirtySourceCount).toBe(1);

		registry.setSourceDirty('matrix-display', false);

		expect(registry.hasChanges).toBe(false);
		expect(registry.dirtySourceCount).toBe(0);
	});

	it('clears a source when its card unmounts', () => {
		const registry = new UnsavedChangesRegistry();

		registry.setSourceDirty('matrix-display', true);
		registry.clearSource('matrix-display');

		expect(registry.hasChanges).toBe(false);
	});
});
