import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';

const { mockSystemStatus } = vi.hoisted(() => {
	const systemStatus = {
		macros: null as ScriptStatus | null,
		subscribeChannel: vi.fn(),
		unsubscribeChannel: vi.fn(),
		subscribeEvents: vi.fn(() => () => {}),
		setMacros: vi.fn((status: ScriptStatus | null) => {
			systemStatus.macros = status;
		})
	};

	return {
		mockSystemStatus: systemStatus
	};
});

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: mockSystemStatus,
	systemEvents: {
		subscribe: vi.fn(() => () => {})
	}
}));

async function mountHook(
	getStatus = vi.fn().mockResolvedValue({
		current_script: '',
		status: 'IDLE',
		current_line: 0,
		uptime_ms: 0,
		last_error: ''
	} satisfies ScriptStatus)
) {
	const { useMacroStatusSync } = await import('./useMacroStatusSync.svelte');
	let sync: ReturnType<typeof useMacroStatusSync> | null = null;
	const cleanup = $effect.root(() => {
		sync = useMacroStatusSync({
			createApi: () => ({ getStatus }),
			channelStore: mockSystemStatus,
			readModel: {
				get status() {
					return mockSystemStatus.macros;
				},
				setStatus(status: ScriptStatus | null) {
					mockSystemStatus.setMacros(status);
				}
			}
		});
	});

	return {
		sync: sync!,
		cleanup,
		getStatus
	};
}

describe('useMacroStatusSync', () => {
	beforeEach(() => {
		vi.resetModules();
		vi.clearAllMocks();
		vi.useFakeTimers();
		mockSystemStatus.macros = null;
	});

	afterEach(() => {
		vi.useRealTimers();
	});

	it('keeps the macros websocket lease across quick remounts', async () => {
		const first = await mountHook();
		await first.sync.init();

		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('macros');

		first.sync.destroy();
		first.cleanup();

		expect(mockSystemStatus.unsubscribeChannel).not.toHaveBeenCalled();

		await vi.advanceTimersByTimeAsync(100);

		const second = await mountHook();
		await second.sync.init();

		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockSystemStatus.unsubscribeChannel).not.toHaveBeenCalled();

		await vi.advanceTimersByTimeAsync(300);
		expect(mockSystemStatus.unsubscribeChannel).not.toHaveBeenCalled();

		second.sync.destroy();
		second.cleanup();

		await vi.advanceTimersByTimeAsync(300);

		expect(mockSystemStatus.unsubscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockSystemStatus.unsubscribeChannel).toHaveBeenCalledWith('macros');
	});
});
