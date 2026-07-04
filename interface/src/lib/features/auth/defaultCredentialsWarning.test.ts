import { beforeEach, describe, expect, it, vi } from 'vitest';

const { mockConfirm } = vi.hoisted(() => ({
	mockConfirm: vi.fn()
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: mockConfirm
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	auth_default_credentials_title: () => 'Default Credentials Detected',
	auth_default_credentials_message: () =>
		'You signed in with the default admin/admin credentials. Change them on the Users page to secure this device.',
	auth_default_credentials_skip: () => 'Skip for now',
	auth_default_credentials_go_to_users: () => 'Go to Users'
}));

describe('openDefaultCredentialsWarning', () => {
	beforeEach(async () => {
		vi.clearAllMocks();
		const { goto } = await import('$app/navigation');
		vi.mocked(goto).mockReset();
	});

	it('opens a confirm dialog with the expected copy and labels', async () => {
		const { openDefaultCredentialsWarning } = await import('./defaultCredentialsWarning');

		openDefaultCredentialsWarning();

		expect(mockConfirm).toHaveBeenCalledWith(
			expect.objectContaining({
				title: 'Default Credentials Detected',
				message:
					'You signed in with the default admin/admin credentials. Change them on the Users page to secure this device.',
				labels: expect.objectContaining({
					cancel: expect.objectContaining({ label: 'Skip for now' }),
					confirm: expect.objectContaining({ label: 'Go to Users' })
				})
			})
		);
	});

	it('navigates to the users page when confirmed and does not navigate on cancel', async () => {
		const { goto } = await import('$app/navigation');
		const { openDefaultCredentialsWarning } = await import('./defaultCredentialsWarning');

		openDefaultCredentialsWarning();

		const options = mockConfirm.mock.calls[0]?.[0] as {
			onConfirm: () => void;
			onCancel: () => void;
		};

		options.onCancel();
		expect(goto).not.toHaveBeenCalled();

		options.onConfirm();
		expect(goto).toHaveBeenCalledWith('/user');
	});
});
