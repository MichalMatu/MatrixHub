import { notifications } from '$lib/components/toasts/notifications.svelte';
import { useSessionAccess, type SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { i18n } from '$lib/i18n.svelte';
import { GpioApiService } from '$lib/services/api/integrations/GpioApiService';
import { Logger } from '$lib/services/core/Logger';
import type {
	GpioChannelConfig,
	GpioChannelStatus,
	GpioConfig,
	GpioPinDefinition
} from '$lib/types/domain/gpio';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import * as m from '$lib/paraglide/messages.js';
import { cloneGpioConfig, gpioConfigsEqual, normalizeChannel, normalizeConfig } from './gpioModel';

const STATUS_POLL_MS = 5000;
const TOAST_MS = 3000;

type GpioApiLike = Pick<
	GpioApiService,
	'getPins' | 'getConfig' | 'saveConfig' | 'getStatus' | 'setOutput'
>;

type GpioManagementDeps = {
	api?: GpioApiLike;
	session?: Pick<SessionAccess, 'canRead' | 'canManage'>;
	notifications?: Pick<typeof notifications, 'success' | 'error'>;
	logger?: Pick<typeof Logger, 'error'>;
	shouldLoad?: () => boolean;
};

export function useGpioManagement(deps: GpioManagementDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const session = deps.session ?? useSessionAccess();
	const toast = deps.notifications ?? notifications;
	const logger = deps.logger ?? Logger;

	function createApi(): GpioApiLike {
		return deps.api ?? apiClient!.createService(GpioApiService);
	}

	let pins = $state<GpioPinDefinition[]>([]);
	let config = $state<GpioConfig | null>(null);
	let savedConfig = $state<GpioConfig | null>(null);
	let status = $state<GpioChannelStatus[]>([]);
	let loading = $state(false);
	let refreshing = $state(false);
	let saving = $state(false);
	let outputBusyId = $state<string | null>(null);
	let error = $state<string | null>(null);
	let statusTimer: ReturnType<typeof setInterval> | undefined;
	let autoLoadArmed = true;

	let statusById = $derived.by(() => {
		const map = new Map<string, GpioChannelStatus>();
		for (const channel of status) {
			map.set(channel.id, channel);
		}
		return map;
	});
	let hasChanges = $derived(!gpioConfigsEqual(config, savedConfig));
	let inputChannels = $derived(
		status.filter((channel) => channel.mode === 'input' && channel.configured)
	);

	function handleError(nextError: unknown, fallbackMessage: string) {
		if (getRequestAbortKind(nextError) === 'abort') return;
		logger.error(fallbackMessage, nextError);
		const message = toUserRequestErrorMessage(nextError, { fallbackMessage });
		error = message;
		toast.error(m.toast_message({ message }, { locale: i18n.languageTag }), TOAST_MS);
	}

	async function refreshPins(api: GpioApiLike = createApi()) {
		if (!session.canRead) return;
		try {
			pins = await api.getPins();
		} catch (nextError) {
			if (getRequestAbortKind(nextError) !== 'abort') {
				logger.error('Failed to refresh GPIO pins:', nextError);
			}
		}
	}

	async function refreshStatus() {
		if (!session.canRead || refreshing) return;
		refreshing = true;
		try {
			const nextStatus = await createApi().getStatus();
			status = nextStatus.channels;
			error = null;
		} catch (nextError) {
			if (getRequestAbortKind(nextError) !== 'abort') {
				logger.error('Failed to refresh GPIO status:', nextError);
				error = toUserRequestErrorMessage(nextError, {
					fallbackMessage: m.gpio_error_load({ locale: i18n.languageTag })
				});
			}
		} finally {
			refreshing = false;
		}
	}

	function startPolling() {
		stopPolling();
		statusTimer = setInterval(() => void refreshStatus(), STATUS_POLL_MS);
	}

	function stopPolling() {
		if (!statusTimer) return;
		clearInterval(statusTimer);
		statusTimer = undefined;
	}

	async function load() {
		if (!session.canRead) return;
		loading = true;
		stopPolling();
		try {
			const api = createApi();
			const nextConfig = await api.getConfig();
			const normalized = normalizeConfig(nextConfig);
			config = cloneGpioConfig(normalized);
			savedConfig = cloneGpioConfig(normalized);
			error = null;
			void refreshPins(api);
			void refreshStatus();
			startPolling();
		} catch (nextError) {
			handleError(nextError, m.gpio_error_load({ locale: i18n.languageTag }));
		} finally {
			loading = false;
		}
	}

	async function save() {
		if (!session.canManage || !config || saving) return false;
		saving = true;
		try {
			const saved = normalizeConfig(await createApi().saveConfig(normalizeConfig(config)));
			config = cloneGpioConfig(saved);
			savedConfig = cloneGpioConfig(saved);
			await refreshStatus();
			toast.success(m.gpio_saved({ locale: i18n.languageTag }), TOAST_MS);
			return true;
		} catch (nextError) {
			handleError(nextError, m.gpio_error_save({ locale: i18n.languageTag }));
			return false;
		} finally {
			saving = false;
		}
	}

	function reset() {
		if (!savedConfig) return;
		config = cloneGpioConfig(savedConfig);
	}

	function updateChannel(id: string, patch: Partial<GpioChannelConfig>) {
		if (!session.canManage || !config) return;
		config = {
			channels: config.channels.map((channel) =>
				channel.id === id ? normalizeChannel({ ...channel, ...patch }) : channel
			)
		};
	}

	async function setOutput(id: string, value: boolean) {
		if (!session.canManage || outputBusyId) return;
		outputBusyId = id;
		try {
			await createApi().setOutput({ id, value });
			if (config) {
				config = {
					channels: config.channels.map((channel) =>
						channel.id === id ? { ...channel, initial_output: value } : channel
					)
				};
			}
			if (savedConfig) {
				savedConfig = {
					channels: savedConfig.channels.map((channel) =>
						channel.id === id ? { ...channel, initial_output: value } : channel
					)
				};
			}
			status = status.map((channel) =>
				channel.id === id
					? {
							...channel,
							logical: value,
							raw: value,
							stable: true,
							changed_at: Date.now(),
							sampled_at: Date.now()
						}
					: channel
			);
			await refreshStatus();
		} catch (nextError) {
			handleError(nextError, m.gpio_error_output({ locale: i18n.languageTag }));
		} finally {
			outputBusyId = null;
		}
	}

	$effect(() => {
		const shouldLoad = deps.shouldLoad?.();
		if (shouldLoad === undefined) return;
		if (!shouldLoad || !session.canRead) {
			autoLoadArmed = true;
			stopPolling();
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;
		void load();
		return () => stopPolling();
	});

	return {
		get pins() {
			return pins;
		},
		get config() {
			return config;
		},
		get status() {
			return status;
		},
		get statusById() {
			return statusById;
		},
		get inputChannels() {
			return inputChannels;
		},
		get loading() {
			return loading;
		},
		get refreshing() {
			return refreshing;
		},
		get saving() {
			return saving;
		},
		get outputBusyId() {
			return outputBusyId;
		},
		get error() {
			return error;
		},
		get hasChanges() {
			return hasChanges;
		},
		get canManage() {
			return session.canManage;
		},
		load,
		refreshStatus,
		save,
		reset,
		updateChannel,
		setOutput
	};
}
