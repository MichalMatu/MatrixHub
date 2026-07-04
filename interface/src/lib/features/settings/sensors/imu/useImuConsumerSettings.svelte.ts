import { AirMouseApiService } from '$lib/services/api/integrations/AirMouseApiService';
import { MatrixApiService, type MatrixSettings } from '$lib/services/api/core/MatrixApiService';
import { Logger } from '$lib/services/core/Logger';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

type ImuConsumerSettingsDeps = {
	shouldLoad?: () => boolean;
};

export function useImuConsumerSettings(deps: ImuConsumerSettingsDeps = {}) {
	const { createService } = useApiClient();

	let matrixSettings = $state<MatrixSettings | null>(null);
	let airMouseStatus = $state<AirMouseStatus | null>(null);
	let loading = $state(false);
	let matrixSaving = $state(false);
	let errorMessage = $state<string | null>(null);
	let autoLoadArmed = true;
	let refreshAbort: AbortController | null = null;

	function active() {
		return deps.shouldLoad?.() ?? true;
	}

	function createMatrixApi() {
		return createService(MatrixApiService);
	}

	function createAirMouseApi() {
		return createService(AirMouseApiService);
	}

	async function refreshConsumers(showLoading = false) {
		refreshAbort?.abort();
		const controller = new AbortController();
		refreshAbort = controller;
		if (showLoading) {
			loading = true;
		}

		try {
			const [matrixResult, airMouseResult] = await Promise.allSettled([
				createMatrixApi().getSettings(),
				createAirMouseApi().getStatus(controller.signal)
			]);

			if (controller.signal.aborted) return;

			if (matrixResult.status === 'fulfilled') {
				matrixSettings = matrixResult.value;
			} else {
				Logger.warn('IMU Matrix consumer settings refresh failed', matrixResult.reason);
			}

			if (airMouseResult.status === 'fulfilled') {
				airMouseStatus = airMouseResult.value;
			} else {
				Logger.warn('IMU AirMouse consumer status refresh failed', airMouseResult.reason);
			}

			if (matrixResult.status === 'rejected' && airMouseResult.status === 'rejected') {
				errorMessage = toUserRequestErrorMessage(matrixResult.reason, {
					fallbackMessage: 'Could not refresh IMU consumers.'
				});
			} else {
				errorMessage = null;
			}
		} finally {
			if (refreshAbort === controller) {
				refreshAbort = null;
			}
			if (showLoading) {
				loading = false;
			}
		}
	}

	async function setMatrixAutoRotate(enabled: boolean) {
		if (matrixSaving) return false;
		matrixSaving = true;
		try {
			matrixSettings = await createMatrixApi().updateSettings({ auto_rotate: enabled });
			errorMessage = null;
			return true;
		} catch (error) {
			if (getRequestAbortKind(error) !== 'abort') {
				Logger.error('Matrix auto-rotate update from IMU page failed', error);
				errorMessage = toUserRequestErrorMessage(error, {
					fallbackMessage: 'Could not update Matrix auto-rotate.'
				});
			}
			return false;
		} finally {
			matrixSaving = false;
		}
	}

	$effect(() => {
		if (!active()) {
			autoLoadArmed = true;
			refreshAbort?.abort();
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;
		void refreshConsumers(true);
	});

	$effect(() => {
		if (!active()) return;
		const interval = window.setInterval(() => {
			void refreshConsumers(false);
		}, 5000);
		return () => {
			window.clearInterval(interval);
			refreshAbort?.abort();
		};
	});

	return {
		get matrixAutoRotate() {
			return matrixSettings?.auto_rotate ?? null;
		},
		get airMouseMovementEnabled() {
			return airMouseStatus?.movement_enabled ?? null;
		},
		get airMouseClickEnabled() {
			return airMouseStatus?.click_enabled ?? null;
		},
		get airMouseRunning() {
			return airMouseStatus?.running ?? null;
		},
		get loading() {
			return loading;
		},
		get matrixSaving() {
			return matrixSaving;
		},
		get errorMessage() {
			return errorMessage;
		},
		refreshConsumers,
		setMatrixAutoRotate
	};
}
