// @vitest-environment jsdom
import { fireEvent, render, screen } from '@testing-library/svelte';
import { describe, expect, it } from 'vitest';
import FormPrimitivesHarness from './FormPrimitivesHarness.svelte';

describe('shared form primitives', () => {
	it('renders shared text input errors and forwards SSID maxlength attributes', () => {
		render(FormPrimitivesHarness);

		const ssidInput = screen.getByTestId('primitives-ssid') as HTMLInputElement;
		expect(ssidInput.maxLength).toBe(32);
		expect(screen.getByRole('textbox', { name: 'SSID' })).toBe(ssidInput);
		expect(screen.getByText('SSID too long (max 32 bytes)')).toBeTruthy();
	});

	it('keeps canonical FormInput features after consolidating to shared/forms', async () => {
		render(FormPrimitivesHarness);

		const passwordInput = screen.getByTestId('primitives-password') as HTMLInputElement;
		expect(passwordInput.type).toBe('password');
		expect(screen.getByText('Hint text')).toBeTruthy();
		expect(screen.getByTestId('suffix-addon')).toBeTruthy();

		await fireEvent.click(screen.getByRole('button', { name: 'Show password' }));

		expect(passwordInput.type).toBe('text');
		expect(screen.getByRole('button', { name: 'Hide password' })).toBeTruthy();
	});

	it('keeps FormSelect accessible and forwards standard HTML attributes', () => {
		render(FormPrimitivesHarness);

		const select = screen.getByTestId('mode-select') as HTMLSelectElement;
		expect(select.value).toBe('b');
		expect(select.className).toContain('select-sm');
		expect(screen.getByRole('combobox', { name: 'Mode' })).toBe(select);
		expect(screen.getByRole('option', { name: 'Mode A' })).toBeTruthy();
		expect(screen.getByRole('option', { name: 'Mode B' })).toBeTruthy();
	});
});
