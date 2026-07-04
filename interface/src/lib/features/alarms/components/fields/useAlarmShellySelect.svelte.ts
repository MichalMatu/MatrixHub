import { onDestroy, onMount } from 'svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import type { ShellyDevice } from '$lib/services/api/integrations/ShellyApiService';
import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
import type { SystemEvent } from '$lib/stores/system/types';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

const LOAD_TIMEOUT_MS = 5000;
const MAX_SHELLY_DEVICES_PER_RULE = 4;

interface ShellyStatusStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
	getSnapshot<TSnapshot>(channel: string): TSnapshot | null;
}

interface ShellyEventBus {
	subscribe(run: (value: SystemEvent | null) => void): () => void;
}

type AlarmShellySelectDeps = {
	statusStore?: ShellyStatusStore;
	eventBus?: ShellyEventBus;
	notifications?: Pick<typeof notifications, 'warning'>;
	timeoutMs?: number;
	setTimer?: typeof setTimeout;
	clearTimer?: typeof clearTimeout;
};

export function useAlarmShellySelect(
	getSelectedIds: () => string[],
	onShellyChange: (ids: string[]) => void,
	deps: AlarmShellySelectDeps = {}
) {
	const statusStore = deps.statusStore;
	const eventBus = deps.eventBus;
	const toast = deps.notifications ?? notifications;
	const setTimer = deps.setTimer ?? setTimeout;
	const clearTimer = deps.clearTimer ?? clearTimeout;
	const timeoutMs = deps.timeoutMs ?? LOAD_TIMEOUT_MS;

	let shellyDevices = $state<ShellyDevice[]>([]);
	let loading = $state(true);
	let error = $state<string | null>(null);
	let loadTimer: ReturnType<typeof setTimeout> | null = null;
	const channelSubscription = createSystemChannelSubscription<ShellyDevice[]>({
		channel: 'shelly',
		onSnapshot: applySnapshot,
		systemStatusStore: statusStore,
		systemEventsBus: eventBus
	});

	function stopLoadTimer() {
		if (loadTimer) {
			clearTimer(loadTimer);
			loadTimer = null;
		}
	}

	function applySnapshot(data: unknown) {
		shellyDevices = Array.isArray(data) ? ((data as ShellyDevice[]) ?? []) : [];
		error = null;
		loading = false;
		stopLoadTimer();
	}

	function startLoadTimer() {
		stopLoadTimer();
		loadTimer = setTimer(() => {
			if (!loading) return;
			loading = false;
			error = m.toast_request_timeout({ locale: i18n.languageTag });
		}, timeoutMs);
	}

	onMount(() => {
		const hasCachedSnapshot = channelSubscription.subscribe();
		if (!hasCachedSnapshot) {
			startLoadTimer();
		}
	});

	onDestroy(() => {
		stopLoadTimer();
		channelSubscription.destroy();
	});

	function toggleDevice(deviceId: string) {
		const selectedIds = getSelectedIds();
		if (selectedIds.includes(deviceId)) {
			onShellyChange(selectedIds.filter((id) => id !== deviceId));
			return;
		}

		if (selectedIds.length >= MAX_SHELLY_DEVICES_PER_RULE) {
			const message = m.toast_shelly_device_limit({ locale: i18n.languageTag });
			toast.warning(m.toast_message({ message }, { locale: i18n.languageTag }), 3000);
			return;
		}

		onShellyChange([...selectedIds, deviceId]);
	}

	return {
		get devices() {
			return shellyDevices;
		},
		get loading() {
			return loading;
		},
		get error() {
			return error;
		},
		toggleDevice
	};
}
