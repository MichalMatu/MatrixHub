import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import Cancel from '~icons/tabler/x';
import Check from '~icons/tabler/check';
import PromptDialog from './PromptDialog.svelte';

const { mockCloseModal } = vi.hoisted(() => ({
	mockCloseModal: vi.fn()
}));

vi.mock('$lib/actions/focusTrap', () => ({
	focusTrap: () => ({
		destroy() {}
	})
}));

vi.mock('$lib/utils/ui/modal', () => ({
	closeModal: mockCloseModal
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	action_cancel: () => 'Cancel',
	action_save: () => 'Save',
	action_close: () => 'Close'
}));

function createModalProps() {
	return {
		id: 'prompt-dialog',
		index: 0,
		close: vi.fn()
	};
}

describe('PromptDialog', () => {
	beforeEach(() => {
		mockCloseModal.mockReset();
	});

	afterEach(() => {
		cleanup();
	});

	it('renders fallback labels, keeps the default value, and confirms the current input', async () => {
		const onConfirm = vi.fn();

		render(PromptDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Edit name',
				message: 'Choose a new name',
				defaultValue: 'Sensor A',
				onConfirm
			}
		});

		const input = screen.getByRole('textbox') as HTMLInputElement;
		expect(input.value).toBe('Sensor A');
		expect(screen.getByRole('button', { name: 'Cancel' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Save' })).toBeTruthy();

		await fireEvent.input(input, { target: { value: 'Sensor B' } });
		await fireEvent.click(screen.getByRole('button', { name: 'Save' }));

		expect(onConfirm).toHaveBeenCalledWith('Sensor B');
		expect(mockCloseModal).toHaveBeenCalledTimes(1);
	}, 10000);

	it('runs cancel flow when closed from the backdrop', async () => {
		const onCancel = vi.fn();

		render(PromptDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Edit name',
				onConfirm: vi.fn(),
				onCancel
			}
		});

		await fireEvent.click(screen.getByRole('dialog'));

		expect(onCancel).toHaveBeenCalledTimes(1);
		expect(mockCloseModal).toHaveBeenCalledTimes(1);
	});

	it('submits on Enter with the latest value', async () => {
		const onConfirm = vi.fn();

		render(PromptDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Edit name',
				defaultValue: 'Sensor A',
				onConfirm
			}
		});

		const input = screen.getByRole('textbox') as HTMLInputElement;
		await fireEvent.input(input, { target: { value: 'Sensor B' } });
		await fireEvent.keyDown(input, { key: 'Enter' });

		expect(onConfirm).toHaveBeenCalledWith('Sensor B');
		expect(mockCloseModal).toHaveBeenCalledTimes(1);
	});

	it('filters non-ascii input before confirming when asciiOnly is enabled', async () => {
		const onConfirm = vi.fn();

		render(PromptDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Edit name',
				asciiOnly: true,
				onConfirm
			}
		});

		const input = screen.getByRole('textbox') as HTMLInputElement;
		await fireEvent.input(input, { target: { value: 'ążabcXYZ' } });

		expect(input.value).toBe('abcXYZ');

		await fireEvent.click(screen.getByRole('button', { name: 'Save' }));

		expect(onConfirm).toHaveBeenCalledWith('abcXYZ');
	});

	it('enforces maxlength before confirming', async () => {
		const onConfirm = vi.fn();

		render(PromptDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Edit name',
				maxlength: 5,
				onConfirm
			}
		});

		const input = screen.getByRole('textbox') as HTMLInputElement;
		await fireEvent.input(input, { target: { value: 'abcdef' } });

		expect(input.value).toBe('abcde');

		await fireEvent.click(screen.getByRole('button', { name: 'Save' }));

		expect(onConfirm).toHaveBeenCalledWith('abcde');
	});

	it('supports custom button labels', () => {
		render(PromptDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Edit name',
				onConfirm: vi.fn(),
				labels: {
					cancel: { label: 'Back', icon: Cancel },
					confirm: { label: 'Apply', icon: Check }
				}
			}
		});

		expect(screen.getByRole('button', { name: 'Back' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Apply' })).toBeTruthy();
	});
});
