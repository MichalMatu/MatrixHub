import { SystemApiService } from '$lib/services/api/core/SystemApiService';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { useLiveTailConfig } from './useLiveTailConfig.svelte';
import { useLiveTailStream } from './useLiveTailStream.svelte';

export function useLiveTailManagement() {
	const { createService } = useApiClient();
	const api = createService(SystemApiService);
	const stream = useLiveTailStream(() => api);
	const config = useLiveTailConfig(() => api);

	function init() {
		void config.refreshLoggingConfig();
	}

	function destroy() {
		stream.stop();
	}

	return {
		get tail() {
			return stream.tail;
		},
		get tailError() {
			return stream.error;
		},
		get capacity() {
			return stream.capacity;
		},
		get isPaused() {
			return stream.isPaused;
		},
		get loggingConfig() {
			return config.loggingConfig;
		},
		get isLoggingConfigLoaded() {
			return config.isLoggingConfigLoaded;
		},
		get savingConfig() {
			return config.savingConfig;
		},
		get configError() {
			return config.error;
		},
		get isDirty() {
			return config.isDirty;
		},
		get levels() {
			return config.levels;
		},
		init,
		destroy,
		start: stream.start,
		stop: stream.stop,
		togglePause: stream.togglePause,
		clear: stream.clear,
		copyToClipboard: stream.copyToClipboard,
		refreshTail: stream.refreshTail,
		refreshLoggingConfig: config.refreshLoggingConfig,
		saveLoggingSettings: config.saveLoggingSettings,
		resetLoggingSettings: config.resetLoggingSettings
	};
}
