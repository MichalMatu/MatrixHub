import { onMount } from 'svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { GpioApiService } from '$lib/services/api/integrations/GpioApiService';
import type { GpioChannelStatus } from '$lib/types/domain/gpio';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

type GpioChannelSelectDeps = {
	api?: Pick<GpioApiService, 'getStatus'>;
};

export function useGpioChannelSelect(
	getSelectedId: () => string | undefined,
	setSelectedId: (value: string) => void,
	deps: GpioChannelSelectDeps = {}
) {
	const apiClient = deps.api ? null : useApiClient();

	function createApi() {
		return deps.api ?? apiClient!.createService(GpioApiService);
	}

	let channels = $state<GpioChannelStatus[]>([]);
	let loading = $state(false);
	let error = $state<string | null>(null);

	async function refresh() {
		loading = true;
		try {
			const status = await createApi().getStatus();
			channels = status.channels.filter(
				(channel) => channel.mode === 'input' && channel.configured
			);
			error = null;
		} catch (nextError) {
			if (getRequestAbortKind(nextError) === 'abort') return;
			error = toUserRequestErrorMessage(nextError, {
				fallbackMessage: m.gpio_error_load({ locale: i18n.languageTag })
			});
		} finally {
			loading = false;
		}
	}

	onMount(() => {
		void refresh();
	});

	$effect(() => {
		if ((getSelectedId() ?? '') === '' && channels.length === 1) {
			setSelectedId(channels[0].id);
		}
	});

	let options = $derived.by(() => [
		{ value: '', label: m.gpio_select_channel({ locale: i18n.languageTag }) },
		...channels.map((channel) => ({
			value: channel.id,
			label: `${channel.name} · GPIO ${channel.pin} · ${
				channel.logical
					? m.gpio_true({ locale: i18n.languageTag })
					: m.gpio_false({ locale: i18n.languageTag })
			}`
		}))
	]);

	let isValidSelection = $derived.by(() => {
		const selected = getSelectedId() ?? '';
		return selected === '' || channels.some((channel) => channel.id === selected);
	});

	return {
		get channels() {
			return channels;
		},
		get loading() {
			return loading;
		},
		get error() {
			return error;
		},
		get options() {
			return options;
		},
		get isValidSelection() {
			return isValidSelection;
		},
		refresh
	};
}
