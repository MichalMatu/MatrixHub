import { describe, expect, it, vi } from 'vitest';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';
import { useMacroStatusReadModel } from './useMacroStatusReadModel.svelte';

describe('useMacroStatusReadModel', () => {
	it('reads and updates macro status through the adapter', () => {
		const initialStatus: ScriptStatus = {
			current_script: '',
			status: 'IDLE',
			current_line: 0,
			uptime_ms: 0,
			last_error: ''
		};
		const runningStatus: ScriptStatus = {
			current_script: 'macro.txt',
			status: 'RUNNING',
			current_line: 4,
			uptime_ms: 200,
			last_error: ''
		};
		const setMacros = vi.fn((nextStatus: ScriptStatus | null) => {
			store.macros = nextStatus;
		});
		const store = {
			macros: initialStatus as ScriptStatus | null,
			setMacros
		};

		const readModel = useMacroStatusReadModel({ store });

		expect(readModel.status?.status).toBe('IDLE');

		readModel.setStatus(runningStatus);

		expect(setMacros).toHaveBeenCalledWith(runningStatus);
		expect(readModel.status?.current_script).toBe('macro.txt');
	});
});
