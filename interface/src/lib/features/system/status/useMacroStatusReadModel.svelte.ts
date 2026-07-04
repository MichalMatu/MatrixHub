import { systemStatus } from '$lib/stores/systemStatus.svelte';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';

type MacroStatusStoreLike = Pick<typeof systemStatus, 'macros' | 'setMacros'>;

interface MacroStatusReadModelDeps {
	store?: MacroStatusStoreLike;
}

export function useMacroStatusReadModel(deps: MacroStatusReadModelDeps = {}) {
	const store = deps.store ?? systemStatus;

	function setStatus(nextStatus: ScriptStatus | null) {
		store.setMacros(nextStatus);
	}

	return {
		get status() {
			return store.macros;
		},
		setStatus
	};
}
