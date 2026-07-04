// @vitest-environment jsdom
import { render, screen } from '@testing-library/svelte';
import { describe, expect, it, vi } from 'vitest';
import LoginForm from './LoginForm.svelte';

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', async (importOriginal) => {
	const actual = await importOriginal<typeof import('$lib/paraglide/messages.js')>();
	return {
		...actual,
		login_username: () => 'Username' as never,
		login_password: () => 'Password' as never,
		login_btn: () => 'Sign In' as never,
		login_pending: () => 'Signing in...' as never
	};
});

describe('LoginForm', () => {
	it('disables the form and keeps loading feedback only on the submit button while submitting', () => {
		render(LoginForm, {
			props: {
				username: 'admin',
				password: 'secret',
				loading: true,
				onSubmit: vi.fn()
			}
		});

		expect(screen.getByLabelText('Username')).toHaveProperty('disabled', true);
		expect(screen.getByLabelText('Password')).toHaveProperty('disabled', true);
		const submitButton = screen.getByRole('button', { name: 'Signing in...' });
		expect(submitButton).toHaveProperty('disabled', true);
		expect(submitButton.querySelector('.loading-spinner')).not.toBeNull();
		expect(screen.queryByRole('status')).toBeNull();
	});
});
