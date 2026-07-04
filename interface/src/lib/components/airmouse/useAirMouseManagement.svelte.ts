import { notifications } from '$lib/components/toasts/notifications.svelte';
import * as m from '$lib/paraglide/messages.js';
import { Logger } from '$lib/services/core/Logger';
import { untrack } from 'svelte';
import { AirMouseApiService } from '$lib/services/api/integrations/AirMouseApiService';
import { MacroApiService, type ScriptFile } from '$lib/services/api/integrations/MacroApiService';
import type { AirMouseConfig, AirMouseStatus } from '$lib/types/devices/airmouse';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { AIR_MOUSE_JIGGLER_DEFAULTS, AIR_MOUSE_JIGGLER_MODES } from './airMouseConfig';
import { useAirMouseConnection } from './useAirMouseConnection.svelte';

type AirMouseApi = Pick<AirMouseApiService, 'calibrate' | 'getStatus' | 'updateConfig'>;
type MacroApi = Pick<MacroApiService, 'getSettings' | 'listScripts'>;

interface AirMouseNotifications {
	success(message: string, duration?: number): void;
	error(message: string, duration?: number): void;
}

interface AirMouseManagementDeps {
	createAirMouseApi?: () => AirMouseApi;
	createMacroApi?: () => MacroApi;
	createConnection?: typeof useAirMouseConnection;
	notifications?: AirMouseNotifications;
	shouldInit?: () => boolean;
}

export function useAirMouseManagement(deps: AirMouseManagementDeps = {}) {
	const { createService } = useApiClient();
	const toast = deps.notifications ?? notifications;

	let status = $state<AirMouseStatus | null>(null);
	let error = $state<string | null>(null);
	let loading = $state(true);
	let scripts = $state<ScriptFile[]>([]);
	let macrosEnabled = $state<boolean | null>(null);
	let calibrating = $state(false);

	let loadAbort: AbortController | null = null;
	let loadRequestId = 0;
	let disposed = false;
	let calibrationRefreshTimer: ReturnType<typeof setTimeout> | null = null;
	let calibrationResetTimer: ReturnType<typeof setTimeout> | null = null;

	const connection =
		deps.createConnection?.(
			() => status,
			(nextStatus) => {
				status = nextStatus;
			}
		) ??
		useAirMouseConnection(
			() => status,
			(nextStatus) => {
				status = nextStatus;
			}
		);

	function createAirMouseApi(): AirMouseApi {
		return deps.createAirMouseApi ? deps.createAirMouseApi() : createService(AirMouseApiService);
	}

	function createMacroApi(): MacroApi {
		return deps.createMacroApi ? deps.createMacroApi() : createService(MacroApiService);
	}

	function init() {
		disposed = false;
		connection.init();
		loading = true;
		void fetchStatus(true);
	}

	function destroy() {
		disposed = true;
		loadAbort?.abort();
		loadAbort = null;
		clearTimers();
		connection.destroy();
	}

	$effect(() => {
		const shouldInit = deps.shouldInit?.() ?? true;
		if (!shouldInit) return;

		untrack(() => {
			init();
		});

		return () => {
			untrack(() => {
				destroy();
			});
		};
	});

	function setUiRequestingConnection(value: boolean) {
		connection.uiRequestingConnection = value;
		connection.updateConnectionState();
	}

	async function refreshMacroContext(controller: AbortController, requestId: number) {
		const macroApi = createMacroApi();
		const [scriptsResult, macroSettingsResult] = await Promise.allSettled([
			macroApi.listScripts(controller.signal),
			macroApi.getSettings(controller.signal)
		]);

		if (disposed || controller.signal.aborted || loadRequestId !== requestId) return;

		if (scriptsResult.status === 'fulfilled') {
			scripts = scriptsResult.value;
		} else {
			Logger.warn('AirMouse scripts metadata refresh failed', scriptsResult.reason);
		}

		if (macroSettingsResult.status === 'fulfilled') {
			macrosEnabled = macroSettingsResult.value?.enabled ?? null;
		} else {
			Logger.warn('AirMouse macro settings refresh failed', macroSettingsResult.reason);
		}
	}

	async function fetchStatus(showLoading = false) {
		loadAbort?.abort();
		const controller = new AbortController();
		loadAbort = controller;
		const requestId = ++loadRequestId;
		if (showLoading) {
			loading = true;
		}

		try {
			const statusData = await createAirMouseApi().getStatus(controller.signal);

			if (disposed || controller.signal.aborted) return;

			status = statusData;
			calibrating = statusData.calibrating;
			error = null;
			connection.updateConnectionState();
			void refreshMacroContext(controller, requestId);
		} catch (err) {
			if (disposed || controller.signal.aborted || getRequestAbortKind(err) === 'abort') return;

			status = null;
			scripts = [];
			macrosEnabled = null;
			calibrating = false;
			error = toUserRequestErrorMessage(err, {
				fallbackMessage: m.settings_load_error()
			});
			connection.stopWebSocket();
			connection.resetHistory();
		} finally {
			if (loadAbort === controller) {
				loadAbort = null;
			}
			if (!disposed && showLoading) {
				loading = false;
			}
		}
	}

	async function calibrate() {
		if (calibrating) return;
		calibrating = true;
		clearCalibrationTimers();

		try {
			await createAirMouseApi().calibrate();
			toast.success(m.airmouse_msg_calib_start(), 3000);
			calibrationRefreshTimer = setTimeout(() => {
				calibrationRefreshTimer = null;
				if (!disposed) {
					void fetchStatus();
				}
			}, 2000);
		} catch (err: unknown) {
			if (getRequestAbortKind(err) === 'abort') return;

			const statusCode =
				typeof err === 'object' && err !== null && 'status' in err
					? (err as { status?: number }).status
					: undefined;

			if (statusCode === 429) {
				toast.error(m.airmouse_msg_calib_cooldown(), 4000);
			} else {
				toast.error(m.airmouse_msg_calib_fail(), 5000);
			}
		} finally {
			calibrationResetTimer = setTimeout(() => {
				calibrationResetTimer = null;
				if (!disposed) {
					calibrating = false;
				}
			}, 1000);
		}
	}

	async function saveSettings(partialSettings: Partial<AirMouseStatus>, notify = true) {
		if (!status) return false;

		try {
			const config: AirMouseConfig = {
				...status,
				jiggler: status.jiggler || getDefaultJiggler(),
				...partialSettings
			};

			await createAirMouseApi().updateConfig(config);
			if (notify) {
				await fetchStatus();
				toast.success(m.airmouse_msg_saving(), 3000);
			}
			return true;
		} catch (err) {
			if (getRequestAbortKind(err) === 'abort') return false;
			if (notify) {
				toast.error(
					toUserRequestErrorMessage(err, {
						fallbackMessage: m.settings_save_error()
					}),
					5000
				);
			}
			throw err;
		}
	}

	function clearTimers() {
		clearCalibrationTimers();
	}

	function clearCalibrationTimers() {
		if (calibrationRefreshTimer) {
			clearTimeout(calibrationRefreshTimer);
			calibrationRefreshTimer = null;
		}

		if (calibrationResetTimer) {
			clearTimeout(calibrationResetTimer);
			calibrationResetTimer = null;
		}
	}

	function getDefaultJiggler() {
		return {
			mode: AIR_MOUSE_JIGGLER_MODES.OFF,
			interval: AIR_MOUSE_JIGGLER_DEFAULTS.interval,
			move_distance: AIR_MOUSE_JIGGLER_DEFAULTS.distance,
			random_interval: AIR_MOUSE_JIGGLER_DEFAULTS.random
		};
	}

	return {
		get status() {
			return status;
		},
		set status(value: AirMouseStatus | null) {
			status = value;
		},
		get error() {
			return error;
		},
		get loading() {
			return loading;
		},
		get wsConnected() {
			return connection.wsConnected;
		},
		get deltaGHistory() {
			return connection.deltaGHistory;
		},
		get gyroXHistory() {
			return connection.gyroXHistory;
		},
		get gyroZHistory() {
			return connection.gyroZHistory;
		},
		get maxHistory() {
			return connection.maxHistory;
		},
		get scripts() {
			return scripts;
		},
		get macrosEnabled() {
			return macrosEnabled;
		},
		get uiRequestingConnection() {
			return connection.uiRequestingConnection;
		},
		set uiRequestingConnection(value: boolean) {
			connection.uiRequestingConnection = value;
		},
		get calibrating() {
			return calibrating;
		},
		get maxDeltaG() {
			return connection.deltaGHistory.length > 0 ? Math.max(...connection.deltaGHistory) : 0;
		},
		updateConnectionState: connection.updateConnectionState,
		fetchStatus,
		calibrate,
		saveSettings,
		setUiRequestingConnection
	};
}
