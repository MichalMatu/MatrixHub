// @vitest-environment jsdom
import { render, screen, waitFor, cleanup } from '@testing-library/svelte';
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import MacroList from './MacroList.svelte';
import * as m from '$lib/paraglide/messages.js';
import { systemStatus } from '$lib/stores/systemStatus.svelte';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const mockApi: any = {
	listScripts: vi.fn(),
	getStatus: vi.fn(),
	runScript: vi.fn(),
	stopScript: vi.fn(),
	deleteScript: vi.fn(),
	uploadScript: vi.fn(),
	baseUrl: '',
	options: {},
	// eslint-disable-next-line @typescript-eslint/no-explicit-any
	client: {} as any
};

const idleStatus: ScriptStatus = {
	current_script: '',
	status: 'IDLE',
	current_line: 0,
	uptime_ms: 0,
	last_error: ''
};

describe('MacroList', () => {
	beforeEach(() => {
		vi.resetAllMocks();
		mockApi.listScripts.mockResolvedValue([{ name: 'test-script.txt' }]);
		mockApi.getStatus.mockResolvedValue(idleStatus);
		systemStatus.setMacros(idleStatus);
	});

	afterEach(() => {
		cleanup();
	});

	it('disables run/stop actions when macros are disabled', async () => {
		render(MacroList, {
			props: { api: mockApi, enabled: false, scripts: [{ name: 'test-script.txt' }] }
		});

		await waitFor(() => {
			expect(screen.getByText('test-script.txt')).toBeTruthy();
		});

		const runButton = screen.getByLabelText(m.macros_action_run());
		const stopButton = screen.getByRole('button', { name: m.macros_stop() });

		expect(runButton.hasAttribute('disabled')).toBe(true);
		expect(stopButton.hasAttribute('disabled')).toBe(true);
	});

	it('enables run action when macros are enabled', async () => {
		render(MacroList, {
			props: { api: mockApi, enabled: true, scripts: [{ name: 'test-script.txt' }] }
		});

		await waitFor(() => {
			expect(screen.getByText('test-script.txt')).toBeTruthy();
		});

		const runButton = screen.getByLabelText(m.macros_action_run());
		expect(runButton.hasAttribute('disabled')).toBe(false);
	});

	it('shows last_error when status is ERROR', async () => {
		const errorStatus: ScriptStatus = {
			current_script: 'bad.txt',
			status: 'ERROR',
			current_line: 1,
			uptime_ms: 0,
			last_error: 'Parse failed'
		};
		systemStatus.setMacros(errorStatus);
		mockApi.getStatus.mockResolvedValue(errorStatus);

		render(MacroList, { props: { api: mockApi, enabled: true } });

		await waitFor(() => {
			expect(screen.getByText('Parse failed')).toBeTruthy();
		});
	});
});
