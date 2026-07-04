import { systemStatus } from '../systemStatus.svelte';

interface SystemTransportStateLike {
	readonly isConnected: boolean;
}

interface SystemTransportStateDeps {
	runtime?: SystemTransportStateLike;
}

export function useSystemTransportState(deps: SystemTransportStateDeps = {}) {
	const runtime = deps.runtime ?? systemStatus;

	return {
		get isConnected() {
			return runtime.isConnected;
		}
	};
}
