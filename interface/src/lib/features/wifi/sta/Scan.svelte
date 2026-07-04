<script lang="ts">
	import Network from '~icons/tabler/router';
	import AP from '~icons/tabler/access-point';
	import Cancel from '~icons/tabler/x';
	import Reload from '~icons/tabler/reload';
	import { onMount } from 'svelte';
	import { Modal } from '$lib/components';
	import { RSSIIndicator as RssiIndicator } from '$lib/components/indicators';
	import { FormButton } from '$lib/components/shared/forms';
	import { closeModal } from '$lib/utils/ui/modal';
	import type { NetworkItem } from '$lib/types/connectivity/wifi';
	import {
		WifiApiService,
		type NetworkListResponse,
		type NetworkScanState
	} from '$lib/services/api/connectivity/WifiApiService';
	import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
	import { usePolling } from '$lib/utils/api/usePolling.svelte';

	import { getEncryptionTypeLabel, WIFI_SCAN_POLL_INTERVAL_MS } from '$lib/constants/wifiConstants';

	// provided by <Modals />
	interface Props {
		isOpen: boolean;
		storeNetwork: (_ssid: string) => void;
	}

	const { isOpen, storeNetwork }: Props = $props();

	let listOfNetworks: NetworkItem[] = $state([]);

	let scanActive = $state(false);
	let scanState = $state<NetworkScanState>('idle');
	let scanError = $state<string | null>(null);
	const apiClient = useApiClient();

	const resultsPoller = usePolling(
		async () => {
			await pollingResults();
		},
		{
			autoStart: false,
			initialPoll: false,
			pauseWhenHidden: true,
			intervalMs: WIFI_SCAN_POLL_INTERVAL_MS,
			jitter: false
		}
	);

	function getApiService(): WifiApiService {
		return apiClient.createService(WifiApiService);
	}

	function resolveScanState(result: NetworkListResponse): NetworkScanState {
		if (result.scan_state) {
			return result.scan_state;
		}

		// Legacy firmware kept polling until at least one result appeared.
		return result.networks.length > 0 ? 'ready' : 'running';
	}

	async function scanNetworks() {
		scanActive = true;
		scanState = 'running';
		scanError = null;
		resultsPoller.stop();
		listOfNetworks = [];
		try {
			await getApiService().scanNetworks();
			if ((await pollingResults()) == false) {
				resultsPoller.start();
			}
		} catch (err) {
			resultsPoller.stop();
			scanActive = false;
			const kind = getRequestAbortKind(err);
			if (kind === 'timeout') {
				scanError = m.wifi_scan_timeout({ locale: i18n.languageTag });
			} else if (kind === 'abort') {
				// Likely navigation/modal close; ignore.
				scanError = null;
			} else {
				scanError = toUserRequestErrorMessage(err, {
					fallbackMessage: m.wifi_scan_failed({ locale: i18n.languageTag })
				});
			}
		}
	}

	async function pollingResults() {
		try {
			const result = await getApiService().listNetworks();
			listOfNetworks = result.networks || [];
			scanState = resolveScanState(result);
			if (scanState !== 'running') {
				scanActive = false;
				resultsPoller.stop();
				return true;
			} else {
				scanActive = true;
				return false;
			}
		} catch (err) {
			const kind = getRequestAbortKind(err);
			if (kind === 'timeout') {
				scanError = m.wifi_scan_timeout({ locale: i18n.languageTag });
				resultsPoller.stop();
				scanActive = false;
			} else if (kind === 'abort') {
				// ignore
			} else {
				scanError = toUserRequestErrorMessage(err, {
					fallbackMessage: m.wifi_scan_results_failed({ locale: i18n.languageTag })
				});
				resultsPoller.stop();
				scanActive = false;
			}
			return false;
		}
	}

	onMount(() => {
		scanNetworks();
	});
</script>

<Modal
	{isOpen}
	onClose={() => closeModal()}
	title={m.wifi_scan_title({ locale: i18n.languageTag })}
	widthClass="w-full min-w-fit max-w-md"
>
	<!-- Content -->
	{#if scanError}
		<div class="alert alert-warning text-sm mb-2">{scanError}</div>
	{/if}
	<div class="overflow-y-auto">
		{#if scanActive}<div class="bg-base-100 flex flex-col items-center justify-center p-6">
				<AP class="text-secondary h-32 w-32 shrink animate-ping stroke-2" />
				<p class="mt-8 text-2xl">{m.wifi_scan_scanning({ locale: i18n.languageTag })}</p>
			</div>
		{:else if listOfNetworks.length > 0}
			<ul class="menu w-full">
				{#each listOfNetworks as network (network.bssid)}
					<li>
						<button
							type="button"
							class="bg-base-200 rounded-btn my-1 flex w-full items-center space-x-3 text-start hover:scale-[1.02] active:scale-[0.98]"
							onclick={() => {
								closeModal();
								storeNetwork(network.ssid);
							}}
						>
							<Network class="text-primary h-6 w-6 shrink-0" />
							<div class="min-w-0 flex-1">
								<div class="font-bold truncate">{network.ssid}</div>
								<div class="text-sm opacity-75 truncate">
									{getEncryptionTypeLabel(network.encryption_type, i18n.languageTag)}, {m.wifi_scan_channel(
										{ channel: network.channel.toString() },
										{ locale: i18n.languageTag }
									)}
								</div>
							</div>
							<RssiIndicator
								showDBm={true}
								rssi_dbm={network.rssi}
								class="text-base-content h-10 w-10 shrink-0"
							/>
						</button>
					</li>
				{/each}
			</ul>
		{:else}
			<div class="bg-base-100 flex flex-col items-center justify-center p-6 text-center">
				<AP class="text-base-content/40 h-16 w-16 shrink-0 stroke-2" />
				<p class="mt-4 text-sm opacity-75">
					{m.wifi_scan_no_results({ locale: i18n.languageTag })}
				</p>
			</div>
		{/if}
	</div>

	<!-- Actions -->
	{#snippet actions()}
		<div class="flex flex-wrap justify-end gap-2 w-full">
			<FormButton
				label={m.action_cancel({ locale: i18n.languageTag })}
				icon={Cancel}
				onclick={() => {
					closeModal();
				}}
				class="btn-neutral"
			/>
			<FormButton
				label={m.wifi_scan_btn_again({ locale: i18n.languageTag })}
				icon={Reload}
				disabled={scanActive}
				onclick={scanNetworks}
				class="btn-primary"
			/>
		</div>
	{/snippet}
</Modal>
