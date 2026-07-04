import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import Check from '~icons/tabler/check';
import InfoDialog from './InfoDialog.svelte';

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
	action_dismiss: () => 'Dismiss',
	action_close: () => 'Close'
}));

describe('InfoDialog', () => {
	beforeEach(() => {
		mockCloseModal.mockReset();
	});

	afterEach(() => {
		cleanup();
	});

	it('renders fallback dismiss label and closes through the shared modal helper', async () => {
		const onDismiss = vi.fn();

		render(InfoDialog, {
			props: {
				isOpen: true,
				title: 'Notice',
				message: 'Plain info message',
				onDismiss
			}
		});

		expect(screen.getByText('Notice')).toBeTruthy();
		expect(screen.getByText('Plain info message')).toBeTruthy();

		await fireEvent.click(screen.getByRole('button', { name: 'Dismiss' }));

		expect(onDismiss).toHaveBeenCalledTimes(1);
		expect(mockCloseModal).toHaveBeenCalledTimes(1);
	});

	it('prefers trusted HTML content and custom dismiss labels', () => {
		render(InfoDialog, {
			props: {
				isOpen: true,
				title: 'Notice',
				message: 'Plain info message',
				messageHtml: '<strong>Trusted info</strong>',
				onDismiss: vi.fn(),
				dismiss: {
					label: 'Close',
					icon: Check
				}
			}
		});

		expect(screen.getByText('Trusted info')).toBeTruthy();
		expect(screen.queryByText('Plain info message')).toBeNull();
		expect(screen.getByText('Close').closest('button')).toBeTruthy();
	});

	it('uses modal dismiss function when provided and skips closeModal fallback', async () => {
		const callOrder: string[] = [];
		const onDismiss = vi.fn(() => {
			callOrder.push('onDismiss');
		});
		const modalDismiss = vi.fn(() => {
			callOrder.push('dismiss');
		});

		render(InfoDialog, {
			props: {
				isOpen: true,
				title: 'Notice',
				message: 'Plain info message',
				onDismiss,
				dismiss: modalDismiss
			}
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Dismiss' }));

		expect(onDismiss).toHaveBeenCalledTimes(1);
		expect(modalDismiss).toHaveBeenCalledTimes(1);
		expect(mockCloseModal).not.toHaveBeenCalled();
		expect(callOrder).toEqual(['onDismiss', 'dismiss']);
	});
});
