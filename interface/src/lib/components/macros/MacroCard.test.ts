// @vitest-environment jsdom
import { render, screen, waitFor } from '@testing-library/svelte';
import { beforeEach, describe, it, expect, vi } from 'vitest';
import * as m from '$lib/paraglide/messages.js';

const { mockMacroApi, mockCreateService } = vi.hoisted(() => ({
	mockMacroApi: {
		getSettings: vi.fn(),
		listScripts: vi.fn(),
		getStatus: vi.fn(),
		getScriptContent: vi.fn(),
		uploadScript: vi.fn(),
		saveSettings: vi.fn(),
		runScript: vi.fn(),
		stopScript: vi.fn(),
		deleteScript: vi.fn()
	},
	mockCreateService: vi.fn()
}));

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

import MacroCard from './MacroCard.svelte';

describe('MacroCard', () => {
	beforeEach(() => {
		mockCreateService.mockReturnValue(mockMacroApi);
		mockMacroApi.getSettings.mockResolvedValue({
			enabled: true,
			boot_script: 'boot.txt',
			boot_delay: 5000
		});
		mockMacroApi.listScripts.mockResolvedValue([]);
		mockMacroApi.getStatus.mockResolvedValue({
			current_script: '',
			status: 'IDLE',
			current_line: 0,
			uptime_ms: 0,
			last_error: ''
		});
	});

	it('keeps boot_script selected when list is empty and disables save', async () => {
		render(MacroCard);

		const select = (await screen.findByRole('combobox')) as HTMLSelectElement;

		await waitFor(() => {
			expect(select.value).toBe('boot.txt');
		});

		const saveButton = screen.getByRole('button', { name: m.action_save() });
		expect(saveButton).toBeTruthy();
		expect(saveButton.hasAttribute('disabled')).toBe(true);
	});
});
