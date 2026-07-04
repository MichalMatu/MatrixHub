import { page } from '$app/state';
import { onDestroy, onMount } from 'svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { connectionState } from '$lib/stores/connectionState.svelte';
import { toUserRequestErrorMessage } from '$lib/utils';
import { confirm } from '$lib/utils/ui/dialogs';
import { defaultModalStack, type ModalStackService } from '$lib/utils/ui/modal';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { useSystemStatusReadModel } from '$lib/features/system/status/useSystemStatusReadModel.svelte';
import { PowerApiService } from '$lib/services/api/core/PowerApiService';
import { resolveFeatureTitle } from './featureRegistry';
import {
	createStatusbarClock,
	getStatusbarConnectionClass,
	getStatusbarTempColor
} from './statusbarModel';
import Cancel from '~icons/tabler/x';
import Power from '~icons/tabler/power';

const TICK_INTERVAL_MS = 1000;

type PowerApi = Pick<PowerApiService, 'requestSleep'>;

interface StatusbarManagementDeps {
	modalStack?: ModalStackService;
	now?: () => number;
	createPowerApi?: () => PowerApi;
}

export function useStatusbarManagement(deps: StatusbarManagementDeps = {}) {
	const modalStack = deps.modalStack ?? defaultModalStack;
	const now = deps.now ?? (() => Date.now());
	const { createService } = useApiClient();
	const statusModel = useSystemStatusReadModel();

	let currentDate = $state('');
	let currentClock = $state('');
	let tickInterval: ReturnType<typeof setInterval> | null = null;

	const status = $derived(statusModel.status);
	const cpuTempStyle = $derived.by(() =>
		statusModel.coreTemp !== undefined ? getStatusbarTempColor(statusModel.coreTemp) : ''
	);
	const connectionStatus = $derived(connectionState.status);
	const connectionClass = $derived.by(() => getStatusbarConnectionClass(connectionStatus));
	const connectionTooltip = $derived.by(() => {
		switch (connectionStatus) {
			case 'connected':
				return m.statusbar_api_connected({ locale: i18n.languageTag });
			case 'connecting':
				return m.statusbar_api_connecting(
					{ attempt: Math.max(1, connectionState.reconnectAttempt) },
					{ locale: i18n.languageTag }
				);
			case 'error':
				return m.statusbar_api_error(
					{
						error:
							connectionState.lastError ?? m.request_error_network({ locale: i18n.languageTag })
					},
					{ locale: i18n.languageTag }
				);
			case 'disconnected':
			default:
				return m.statusbar_api_disconnected({ locale: i18n.languageTag });
		}
	});

	const currentTitle = $derived.by(() => {
		if (page.data?.title && page.data?.titleOverride) {
			return page.data.title;
		}

		return (
			resolveFeatureTitle(page.url.pathname, i18n.languageTag) ??
			page.data?.title ??
			m.app_title({ locale: i18n.languageTag })
		);
	});

	function createPowerApi() {
		return deps.createPowerApi?.() ?? createService(PowerApiService);
	}

	function updateClock() {
		const nextClock = createStatusbarClock(status, now());
		currentDate = nextClock.date;
		currentClock = nextClock.clock;
	}

	async function postSleep() {
		try {
			await createPowerApi().requestSleep();
		} catch (error) {
			const message = toUserRequestErrorMessage(error, {
				fallbackMessage: m.sleep_request_failed({ locale: i18n.languageTag })
			});
			notifications.error(m.error_prefix({ error: message }, { locale: i18n.languageTag }), 4000);
		}
	}

	function confirmSleep() {
		confirm({
			title: m.sleep_confirm_title({ locale: i18n.languageTag }),
			message: m.sleep_confirm_msg({ locale: i18n.languageTag }),
			labels: {
				cancel: { label: m.sleep_abort_btn({ locale: i18n.languageTag }), icon: Cancel },
				confirm: { label: m.sleep_enter_btn({ locale: i18n.languageTag }), icon: Power }
			},
			onConfirm: () => {
				void postSleep();
			},
			modalService: modalStack
		});
	}

	onMount(() => {
		updateClock();
		tickInterval = setInterval(updateClock, TICK_INTERVAL_MS);
	});

	onDestroy(() => {
		if (tickInterval) {
			clearInterval(tickInterval);
			tickInterval = null;
		}
	});

	return {
		get status() {
			return status;
		},
		get currentDate() {
			return currentDate;
		},
		get currentClock() {
			return currentClock;
		},
		get currentTitle() {
			return currentTitle;
		},
		get cpuTempStyle() {
			return cpuTempStyle;
		},
		get connectionStatus() {
			return connectionStatus;
		},
		get connectionClass() {
			return connectionClass;
		},
		get connectionTooltip() {
			return connectionTooltip;
		},
		confirmSleep
	};
}
