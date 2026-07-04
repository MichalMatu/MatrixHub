<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { WifiStatus } from '$lib/types/connectivity/wifi';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	// Icons
	import AP from '~icons/tabler/access-point';
	import Router from '~icons/tabler/router';
	import MAC from '~icons/tabler/dna-2';
	import Home from '~icons/tabler/home';
	import WiFi from '~icons/tabler/wifi';
	import SSID from '~icons/tabler/router';
	import Down from '~icons/tabler/chevron-down';
	import DNS from '~icons/tabler/address-book';
	import Gateway from '~icons/tabler/torii';
	import Subnet from '~icons/tabler/grid-dots';
	import Channel from '~icons/tabler/antenna';

	interface Props {
		status: WifiStatus;
	}

	let { status }: Props = $props();

	let showDetails = $state(false);

	const statusIconClass = (active: boolean) =>
		`h-6 w-6 flex-none ${active ? 'text-success' : 'text-error'}`;
	const detailIconClass = 'h-6 w-6 flex-none text-base-content/70';

	const isConnected = $derived(status.status === 3);
</script>

<BaseCard title={m.status_wifi_title({ locale: i18n.languageTag })} icon={Router} class="h-full">
	{#snippet actions()}
		<div class="badge badge-lg {isConnected ? 'badge-success' : 'badge-outline'}">
			{isConnected
				? m.status_wifi_connected_default({ locale: i18n.languageTag })
				: m.status_wifi_disconnected({ locale: i18n.languageTag })}
		</div>
	{/snippet}

	<div class="flex flex-col gap-1">
		<div class="flex flex-col gap-1" transition:slide|local={{ duration: 300, easing: cubicOut }}>
			<StatusRow
				icon={AP}
				iconClass={statusIconClass(isConnected)}
				label={m.wifi_stat_status({ locale: i18n.languageTag })}
				value={isConnected
					? m.status_wifi_connected_default({ locale: i18n.languageTag })
					: m.status_wifi_disconnected({ locale: i18n.languageTag })}
			/>

			{#if isConnected}
				<StatusRow
					icon={SSID}
					iconClass={detailIconClass}
					label={m.wifi_stat_ssid({ locale: i18n.languageTag })}
					value={status.ssid}
				/>

				<StatusRow
					icon={Home}
					iconClass={detailIconClass}
					label={m.wifi_stat_ip({ locale: i18n.languageTag })}
					value={status.local_ip}
				/>

				<StatusRow
					icon={WiFi}
					iconClass={detailIconClass}
					label={m.wifi_stat_rssi({ locale: i18n.languageTag })}
					value={`${status.rssi} dBm`}
				>
					{#snippet actions()}
						<button
							type="button"
							class="btn btn-circle btn-ghost btn-sm modal-button"
							onclick={() => {
								showDetails = !showDetails;
							}}
							aria-label={showDetails
								? m.action_hide_details({ locale: i18n.languageTag })
								: m.action_show_details({ locale: i18n.languageTag })}
							title={showDetails
								? m.action_hide_details({ locale: i18n.languageTag })
								: m.action_show_details({ locale: i18n.languageTag })}
						>
							<Down
								class="text-base-content h-auto w-6 transition-transform duration-300 ease-in-out {showDetails
									? 'rotate-180'
									: ''}"
							/>
						</button>
					{/snippet}
				</StatusRow>

				<!-- Expandable Details -->
				{#if showDetails}
					<div
						class="flex w-full flex-col gap-1 pt-1"
						transition:slide|local={{ duration: 300, easing: cubicOut }}
					>
						<StatusRow
							icon={MAC}
							iconClass={detailIconClass}
							label={m.wifi_stat_mac({ locale: i18n.languageTag })}
							value={status.mac_address}
						/>

						<StatusRow
							icon={Channel}
							iconClass={detailIconClass}
							label={m.wifi_stat_channel({ locale: i18n.languageTag })}
							value={status.channel}
						/>

						<StatusRow
							icon={Gateway}
							iconClass={detailIconClass}
							label={m.wifi_stat_gateway({ locale: i18n.languageTag })}
							value={status.gateway_ip}
						/>

						<StatusRow
							icon={Subnet}
							iconClass={detailIconClass}
							label={m.wifi_stat_mask({ locale: i18n.languageTag })}
							value={status.subnet_mask}
						/>

						<StatusRow
							icon={DNS}
							iconClass={detailIconClass}
							label={m.wifi_stat_dns({ locale: i18n.languageTag })}
							value={status.dns_ip_1}
						/>
					</div>
				{/if}
			{/if}
		</div>
	</div>
</BaseCard>
