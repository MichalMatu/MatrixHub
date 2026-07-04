<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { KnownNetworkItem } from '$lib/types/connectivity/wifi';
	import { useNetworkList } from '$lib/features/wifi/sta/useNetworkList.svelte';
	import { validateWifiNetwork } from '$lib/features/wifi/wifiValidation';

	// Icons
	import Router from '~icons/tabler/router';
	import Add from '~icons/tabler/circle-plus';
	import Edit from '~icons/tabler/pencil';
	import Delete from '~icons/tabler/trash';
	import ArrowUp from '~icons/tabler/arrow-up';
	import ArrowDown from '~icons/tabler/arrow-down';
	import Scan from '~icons/tabler/radar-2';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	interface Props {
		networks: KnownNetworkItem[];
		onNetworksChange: (_networks: KnownNetworkItem[]) => void;
		isDirty: boolean;
		saveBlocked?: boolean;
		onApply: () => void;
		onReset: () => void;
	}

	let {
		networks,
		onNetworksChange,
		isDirty,
		saveBlocked = false,
		onApply,
		onReset
	}: Props = $props();

	const listLogic = useNetworkList(
		() => networks,
		() => onNetworksChange
	);

	const detailIconClass = 'h-5 w-5 flex-none text-base-content/70';
	const invalidNetworkIssues = $derived.by(() => {
		i18n.languageTag;

		return networks.map((network) => {
			const result = validateWifiNetwork(network);
			if (result.success) return null;

			const messages = Array.from(
				new Set(
					Object.values(result.error.flatten().fieldErrors)
						.flat()
						.filter((message): message is string => Boolean(message))
				)
			);

			return { messages };
		});
	});
	const invalidNetworkCount = $derived.by(
		() => invalidNetworkIssues.filter((issue) => issue !== null).length
	);
</script>

<SettingsCard
	title={m.wifi_sta_saved_networks({ locale: i18n.languageTag })}
	icon={Router}
	class="h-full"
	hasChanges={isDirty}
	disabled={saveBlocked}
	onSave={onApply}
	{onReset}
	dirtySourceId="wifi-saved-networks"
>
	{#snippet actions()}
		<div class="flex gap-x-2">
			<FormButton
				label=""
				icon={Add}
				onclick={() => listLogic.handleNewNetwork()}
				class="btn-primary text-primary-content btn-xs sm:btn-sm"
				title={m.wifi_network_add_title({ locale: i18n.languageTag })}
				aria-label={m.wifi_network_add_title({ locale: i18n.languageTag })}
			/>
			<FormButton
				label=""
				icon={Scan}
				onclick={() => {
					if (!listLogic.isNetworkListTooLong()) {
						listLogic.scanForNetworks();
					}
				}}
				class="btn-primary text-primary-content btn-xs sm:btn-sm"
				title={m.wifi_scan_title({ locale: i18n.languageTag })}
				aria-label={m.wifi_scan_title({ locale: i18n.languageTag })}
			/>
		</div>
	{/snippet}

	<div class="flex flex-col h-full">
		<div class="flex-1" transition:slide|local={{ duration: 300, easing: cubicOut }}>
			{#if invalidNetworkCount > 0}
				<div class="alert alert-warning text-sm mb-2">
					<AlertTriangle class="h-4 w-4" />
					<span>{m.wifi_saved_networks_invalid({ locale: i18n.languageTag })}</span>
				</div>
			{/if}

			{#if networks.length === 0}
				<div class="text-center text-base-content/50 mt-2">
					{m.wifi_sta_no_saved({ locale: i18n.languageTag })}
				</div>
			{:else}
				<div class="flex flex-col gap-2">
					{#each networks as network, index (network.ssid)}
						{@const networkIssue = invalidNetworkIssues[index]}
						<div transition:slide|local={{ duration: 200 }}>
							<StatusRow
								paddingClass="p-2"
								label={network.ssid}
								labelClass="font-bold truncate"
								icon={Router}
								iconClass={detailIconClass}
							>
								{#snippet leading()}
									<div class="flex flex-col gap-1">
										<FormButton
											label=""
											icon={ArrowUp}
											disabled={index === 0}
											onclick={() => listLogic.moveNetwork(index, 'up')}
											class="btn-xs btn-ghost btn-square"
											title={m.wifi_move_up({ locale: i18n.languageTag })}
											aria-label={m.wifi_move_up({ locale: i18n.languageTag })}
										/>
										<FormButton
											label=""
											icon={ArrowDown}
											disabled={index === networks.length - 1}
											onclick={() => listLogic.moveNetwork(index, 'down')}
											class="btn-xs btn-ghost btn-square"
											title={m.wifi_move_down({ locale: i18n.languageTag })}
											aria-label={m.wifi_move_down({ locale: i18n.languageTag })}
										/>
									</div>
								{/snippet}
								{#snippet labelAddon()}
									{#if networkIssue}
										<div
											class="badge badge-xs badge-error text-error-content flex-shrink-0"
											title={networkIssue.messages.join(' | ')}
										>
											{m.wifi_invalid_badge({ locale: i18n.languageTag })}
										</div>
									{/if}
									{#if network.static_ip_config}
										<div
											class="badge badge-xs badge-secondary opacity-75 flex-shrink-0 hidden sm:block"
										>
											{m.wifi_ip_static({ locale: i18n.languageTag })}
										</div>
									{:else}
										<div
											class="badge badge-xs badge-outline badge-secondary opacity-75 flex-shrink-0 hidden sm:block"
										>
											{m.wifi_ip_dhcp({ locale: i18n.languageTag })}
										</div>
									{/if}
								{/snippet}
								{#snippet actions()}
									<FormButton
										label=""
										icon={Edit}
										onclick={() => listLogic.handleEdit(index)}
										class="btn-ghost btn-sm"
										title={m.action_edit({ locale: i18n.languageTag })}
										aria-label={m.action_edit({ locale: i18n.languageTag })}
									/>
									<FormButton
										label=""
										icon={Delete}
										onclick={() => listLogic.confirmDelete(index)}
										class="btn-ghost btn-sm text-error"
										title={m.action_delete({ locale: i18n.languageTag })}
										aria-label={m.action_delete({ locale: i18n.languageTag })}
									/>
								{/snippet}
							</StatusRow>

							{#if networkIssue}
								<div class="px-2 pt-1 text-xs text-error break-words">
									{networkIssue.messages.join(' ')}
								</div>
							{/if}
						</div>
					{/each}
				</div>
			{/if}
		</div>
	</div>
</SettingsCard>
