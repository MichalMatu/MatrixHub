import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import ModalTestHarness from './ModalTestHarness.svelte';

vi.mock('$lib/actions/focusTrap', () => ({
	focusTrap: () => ({
		destroy() {}
	})
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	action_close: () => 'Close'
}));

describe('Modal', () => {
	afterEach(() => {
		cleanup();
	});

	it('renders the shared title, body, and actions shell', () => {
		render(ModalTestHarness, {
			props: {
				onClose: vi.fn()
			}
		});

		expect(screen.getByText('Test modal')).toBeTruthy();
		expect(screen.getByText('Modal body')).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Action' })).toBeTruthy();
		expect(screen.getByRole('button', { name: 'Close' })).toBeTruthy();
	});

	it('does not close when clicking inside the modal surface', async () => {
		const onClose = vi.fn();
		render(ModalTestHarness, {
			props: {
				onClose
			}
		});

		await fireEvent.click(screen.getByText('Modal body'));

		expect(onClose).not.toHaveBeenCalled();
	});

	it('closes from the shared close button', async () => {
		const onClose = vi.fn();
		render(ModalTestHarness, {
			props: {
				onClose
			}
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Close' }));

		expect(onClose).toHaveBeenCalledTimes(1);
	});

	it('closes on Escape', async () => {
		const onClose = vi.fn();
		render(ModalTestHarness, {
			props: {
				onClose
			}
		});

		await fireEvent.keyDown(screen.getByRole('dialog'), { key: 'Escape' });

		expect(onClose).toHaveBeenCalledTimes(1);
	});

	it('closes on backdrop click by default', async () => {
		const onClose = vi.fn();
		render(ModalTestHarness, {
			props: {
				onClose
			}
		});

		await fireEvent.click(screen.getByRole('dialog'));

		expect(onClose).toHaveBeenCalledTimes(1);
	});

	it('does not close on backdrop click when disabled', async () => {
		const onClose = vi.fn();
		render(ModalTestHarness, {
			props: {
				onClose,
				closeOnOutsideClick: false
			}
		});

		await fireEvent.click(screen.getByRole('dialog'));

		expect(onClose).not.toHaveBeenCalled();
	});
});
