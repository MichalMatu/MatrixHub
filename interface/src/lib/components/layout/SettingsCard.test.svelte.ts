// @vitest-environment jsdom
import { fireEvent, render, screen, waitFor } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import SettingsCardTestHarness from './SettingsCardTestHarness.svelte';
import { unsavedChanges } from '$lib/stores/unsavedChanges.svelte';

vi.mock('$lib/paraglide/messages.js', () => ({
	action_discard: () => 'Discard',
	action_save: () => 'Save',
	settings_title: () => 'Settings'
}));

describe('SettingsCard', () => {
	afterEach(() => {
		unsavedChanges.clearAllForTests();
	});

	it('disables save and discard while the card has no changes', () => {
		const onSave = vi.fn();
		const onReset = vi.fn();
		render(SettingsCardTestHarness, {
			props: { hasChanges: false, onSave, onReset }
		});

		const saveButton = screen.getByRole('button', { name: 'Save' }) as HTMLButtonElement;
		const discardButton = screen.getByRole('button', { name: 'Discard' }) as HTMLButtonElement;

		expect(saveButton.disabled).toBe(true);
		expect(discardButton.disabled).toBe(true);
	});

	it('resets the draft through the shared discard action', async () => {
		const onReset = vi.fn();
		render(SettingsCardTestHarness, {
			props: { hasChanges: true, onReset }
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Discard' }));

		expect(onReset).toHaveBeenCalledOnce();
	});

	it('guards the save action against a double click while saving', async () => {
		let resolveSave: (() => void) | undefined;
		const onSave = vi.fn(
			() =>
				new Promise<void>((resolve) => {
					resolveSave = resolve;
				})
		);
		render(SettingsCardTestHarness, {
			props: { hasChanges: true, onSave }
		});

		const saveButton = screen.getByRole('button', { name: 'Save' });
		void fireEvent.click(saveButton);
		await fireEvent.click(saveButton);

		expect(onSave).toHaveBeenCalledOnce();
		resolveSave?.();
	});

	it('registers and clears dirty state for the global navigation guard', async () => {
		const view = render(SettingsCardTestHarness, {
			props: { hasChanges: true, dirtySourceId: 'settings-card-test' }
		});

		await waitFor(() => expect(unsavedChanges.hasChanges).toBe(true));
		await view.rerender({ hasChanges: false, dirtySourceId: 'settings-card-test' });
		await waitFor(() => expect(unsavedChanges.hasChanges).toBe(false));

		await view.rerender({ hasChanges: true, dirtySourceId: 'settings-card-test' });
		await waitFor(() => expect(unsavedChanges.hasChanges).toBe(true));

		view.unmount();
		await waitFor(() => expect(unsavedChanges.hasChanges).toBe(false));
	});
});
