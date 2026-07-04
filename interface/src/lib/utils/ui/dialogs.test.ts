import { beforeEach, describe, expect, it, vi } from 'vitest';

const { mockModals } = vi.hoisted(() => ({
	mockModals: {
		open: vi.fn(),
		close: vi.fn(),
		closeById: vi.fn()
	}
}));

const confirmDialogComponent = {};
const infoDialogComponent = {};
const promptDialogComponent = {};
const customModalComponent = {};

vi.mock('svelte-modals', () => ({
	modals: mockModals
}));

vi.mock('$lib/components', () => ({
	ConfirmDialog: confirmDialogComponent,
	InfoDialog: infoDialogComponent,
	PromptDialog: promptDialogComponent
}));

describe('modal helpers', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('openModal forwards component, props, and options to svelte-modals', async () => {
		const { openModal } = await import('./modal');

		openModal(customModalComponent, { title: 'Custom' }, mockModals as never, { id: 'custom-id' });

		expect(mockModals.open).toHaveBeenCalledWith(
			customModalComponent,
			{ title: 'Custom' },
			{ id: 'custom-id' }
		);
	});

	it('close helpers forward to the shared modal stack', async () => {
		const { closeModal, closeModalLayers, closeModalById } = await import('./modal');

		closeModal(mockModals as never);
		closeModalLayers(2, mockModals as never);
		closeModalById('dialog-id', mockModals as never);

		expect(mockModals.close).toHaveBeenCalledWith();
		expect(mockModals.close).toHaveBeenCalledWith(2);
		expect(mockModals.closeById).toHaveBeenCalledWith('dialog-id');
	});

	it('confirm opens the shared confirm dialog with forwarded props', async () => {
		const { confirm } = await import('./dialogs');

		confirm({
			title: 'Delete device',
			message: 'Are you sure?',
			onConfirm: vi.fn()
		});

		expect(mockModals.open).toHaveBeenCalledWith(
			confirmDialogComponent,
			expect.objectContaining({
				title: 'Delete device',
				message: 'Are you sure?'
			})
		);
	});

	it('info opens the shared info dialog with dismiss props', async () => {
		const { info } = await import('./dialogs');
		const onDismiss = vi.fn();

		info({
			title: 'Limit reached',
			message: 'Too many items',
			onDismiss
		});

		expect(mockModals.open).toHaveBeenCalledWith(
			infoDialogComponent,
			expect.objectContaining({
				title: 'Limit reached',
				message: 'Too many items',
				onDismiss
			})
		);
	});

	it('prompt opens the shared prompt dialog with input props', async () => {
		const { prompt } = await import('./dialogs');
		const onConfirm = vi.fn();

		prompt({
			title: 'Edit name',
			message: 'Provide alias',
			defaultValue: 'Sensor',
			maxlength: 10,
			onConfirm
		});

		expect(mockModals.open).toHaveBeenCalledWith(
			promptDialogComponent,
			expect.objectContaining({
				title: 'Edit name',
				message: 'Provide alias',
				defaultValue: 'Sensor',
				maxlength: 10,
				onConfirm
			})
		);
	});
});
