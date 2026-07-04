import { fireEvent, render, screen } from '@testing-library/svelte';
import { describe, expect, it, vi } from 'vitest';
import EditUser from './EditUser.svelte';

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
	user_field_username: () => 'Username',
	user_error_username_len: () => 'Username length invalid',
	user_field_password: () => 'Password',
	user_form_admin: () => 'Admin',
	user_form_admin_full: () => 'Full access',
	user_form_admin_limited: () => 'Limited access',
	action_show_password: () => 'Show password',
	action_hide_password: () => 'Hide password',
	action_cancel: () => 'Cancel',
	action_save: () => 'Save',
	action_close: () => 'Close'
}));

describe('EditUser', () => {
	it('submits only once when the save button is clicked', async () => {
		const onSaveUser = vi.fn();
		mockCloseModal.mockReset();

		render(EditUser, {
			props: {
				isOpen: true,
				title: 'Edit user',
				onSaveUser,
				user: {
					username: 'alice',
					password: 'secret',
					admin: true
				}
			}
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Save' }));

		expect(onSaveUser).toHaveBeenCalledTimes(1);
		expect(onSaveUser).toHaveBeenCalledWith({
			username: 'alice',
			password: 'secret',
			admin: true
		});
		expect(mockCloseModal).toHaveBeenCalledTimes(1);
	});
});
