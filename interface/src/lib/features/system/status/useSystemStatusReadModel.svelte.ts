import { systemStatus } from '$lib/stores/systemStatus.svelte';
import type { SystemStatus } from '$lib/types/system/systemStatus';

type SystemStatusStoreLike = Pick<typeof systemStatus, 'data'>;

interface SystemStatusReadModelDeps {
	store?: SystemStatusStoreLike;
}

export function useSystemStatusReadModel(deps: SystemStatusReadModelDeps = {}) {
	const store = deps.store ?? systemStatus;

	return {
		get status(): SystemStatus {
			return store.data;
		},
		get coreTemp() {
			return store.data.coreTemp;
		},
		get isStaConnected() {
			const status = store.data;
			return status.isStaConnected ?? status.isConnected;
		}
	};
}
