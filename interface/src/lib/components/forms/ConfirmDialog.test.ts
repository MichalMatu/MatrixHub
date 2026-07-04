import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import Cancel from '~icons/tabler/x';
import Check from '~icons/tabler/check';
import ConfirmDialog from './ConfirmDialog.svelte';

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
	action_confirm: () => 'Confirm',
	action_close: () => 'Close'
}));

function createModalProps() {
	return {
		id: 'confirm-dialog',
		index: 0,
		close: vi.fn()
	};
}

describe('ConfirmDialog', () => {
	beforeEach(() => {
		mockCloseModal.mockReset();
	});

	afterEach(() => {
		cleanup();
	});

	it('renders fallback labels and confirms through the primary action', async () => {
		const onConfirm = vi.fn();

		render(ConfirmDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Delete device',
				message: 'Are you sure?',
				onConfirm
			}
		});

		expect(screen.getByText('Delete device')).toBeTruthy();
		expect(screen.getByText('Are you sure?')).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Cancel' })).toBeTruthy();

		await fireEvent.click(screen.getByRole('button', { name: 'Confirm' }));

		expect(onConfirm).toHaveBeenCalledTimes(1);
		expect(mockCloseModal).toHaveBeenCalledTimes(1);
	});

	it('routes modal close through the cancel handler', async () => {
		const onCancel = vi.fn();

		render(ConfirmDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Delete device',
				message: 'Are you sure?',
				onConfirm: vi.fn(),
				onCancel
			}
		});

		await fireEvent.click(screen.getByRole('dialog'));

		expect(onCancel).toHaveBeenCalledTimes(1);
		expect(mockCloseModal).toHaveBeenCalledTimes(1);
	});

	it('prefers trusted HTML content and supports custom button labels', () => {
		render(ConfirmDialog, {
			props: {
				...createModalProps(),
				isOpen: true,
				title: 'Delete device',
				message: 'Plain text fallback',
				messageHtml: '<strong>Trusted HTML</strong>',
				onConfirm: vi.fn(),
				labels: {
					cancel: { label: 'Back', icon: Cancel },
					confirm: { label: 'Delete', icon: Check }
				}
			}
		});

		expect(screen.getByText('Trusted HTML')).toBeTruthy();
		expect(screen.queryByText('Plain text fallback')).toBeNull();
		expect(screen.getByRole('button', { name: 'Back' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Delete' })).toBeTruthy();
	});
});
