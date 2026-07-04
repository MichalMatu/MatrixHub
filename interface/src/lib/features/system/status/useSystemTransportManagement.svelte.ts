import { systemStatus } from '$lib/stores/systemStatus.svelte';

type SystemTransportStoreLike = Pick<
	typeof systemStatus,
	'subscribeChannel' | 'unsubscribeChannel'
>;

interface SystemTransportManagementDeps {
	store?: SystemTransportStoreLike;
}

export function useSystemTransportManagement(deps: SystemTransportManagementDeps = {}) {
	const store = deps.store ?? systemStatus;

	return {
		subscribeChannel(channel: string) {
			store.subscribeChannel(channel);
		},
		unsubscribeChannel(channel: string) {
			store.unsubscribeChannel(channel);
		}
	};
}
