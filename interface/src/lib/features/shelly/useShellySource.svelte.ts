import { onMount, untrack } from 'svelte';
import {
	ShellyApiService,
	type ShellyDevice
} from '$lib/services/api/integrations/ShellyApiService';
import { useSessionAccess, type SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { Logger } from '$lib/services/core/Logger';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { shellyStore, type ShellyStore } from '$lib/stores/shelly.svelte';

type ShellySourceDeps = {
	initialDevicesFn?: () => ShellyDevice[];
	access?: Pick<SessionAccess, 'apiOptions' | 'canRead' | 'canManage'>;
	createApi?: () => Pick<
		ShellyApiService,
		'getDevices' | 'addDevice' | 'deleteDevice' | 'controlDevice'
	>;
	store?: ShellyStore;
};

export function useShellySource(deps: ShellySourceDeps = {}) {
	const access = deps.access ?? useSessionAccess();
	const store = deps.store ?? shellyStore;
	const initialDevices = deps.initialDevicesFn?.() ?? [];
	let started = false;
	let mounted = false;

	function createApi() {
		return deps.createApi?.() ?? new ShellyApiService(access.apiOptions);
	}

	async function loadDevicesFallback() {
		try {
			store.applySnapshot(await createApi().getDevices());
		} catch (error) {
			Logger.error('Failed to load Shelly devices:', error);
			if (getRequestAbortKind(error) === 'abort') return;
			store.setError(
				toUserRequestErrorMessage(error, {
					fallbackMessage: m.toast_shelly_device_load_failed({ locale: i18n.languageTag })
				})
			);
		}
	}

	function start() {
		if (started) {
			return;
		}

		if (initialDevices.length > 0 && store.devices.length === 0) {
			store.hydrateDevices(initialDevices);
		}

		if (store.devices.length === 0 && store.errorMessage === null) {
			store.setLoading(true);
		}

		const hasCachedSnapshot = store.start();
		started = true;

		if (!hasCachedSnapshot && store.loading) {
			void loadDevicesFallback();
		}
	}

	function stop() {
		if (!started) return;
		store.stop();
		started = false;
	}

	function syncAccessState() {
		if (!access.canRead) {
			stop();
			store.setLoading(false);
			return;
		}

		start();
	}

	onMount(() => {
		mounted = true;
		syncAccessState();

		return () => {
			mounted = false;
			stop();
		};
	});

	$effect(() => {
		const _ = access.canRead;
		if (!mounted) return;

		untrack(() => {
			syncAccessState();
		});
	});

	async function persistDevice(
		device: ShellyDevice,
		options: {
			isEditing: boolean;
			optimisticDevice?: ShellyDevice;
		}
	) {
		const { isEditing, optimisticDevice = device } = options;
		await createApi().addDevice(device);
		store.upsertDevice(optimisticDevice);
		notifications.success(
			isEditing
				? m.toast_shelly_device_updated({ locale: i18n.languageTag })
				: m.toast_shelly_device_added({ locale: i18n.languageTag }),
			3000
		);
	}

	async function deleteDevice(id: string) {
		await createApi().deleteDevice(id);
		store.removeDevice(id);
		notifications.success(m.toast_shelly_device_deleted({ locale: i18n.languageTag }), 3000);
	}

	async function toggleDevice(id: string, newState: boolean) {
		const previousState = store.devices.find((device) => device.id === id)?.isOn;
		store.updateToggle(id, newState);

		try {
			await createApi().controlDevice({ id, on: newState });
			store.setError(null);
		} catch (error) {
			Logger.error('Failed to toggle Shelly device:', error);
			if (previousState !== undefined) {
				store.updateToggle(id, previousState);
			}
			if (getRequestAbortKind(error) === 'abort') return;
			const message = toUserRequestErrorMessage(error, {
				fallbackMessage: m.shelly_error_toggle({ locale: i18n.languageTag })
			});
			store.setError(message);
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 4000);
		}
	}

	return {
		get devices() {
			return store.devices;
		},
		get loading() {
			return store.loading;
		},
		get errorMessage() {
			return store.errorMessage;
		},
		get canManage() {
			return access.canManage;
		},
		get canRead() {
			return access.canRead;
		},
		loadDevicesFallback,
		persistDevice,
		deleteDevice,
		toggleDevice,
		start,
		stop
	};
}
