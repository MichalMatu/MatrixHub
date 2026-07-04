<script lang="ts">
	import Heart from '~icons/tabler/heart';
	import HeartBroken from '~icons/tabler/heart-broken';
	import Refresh from '~icons/tabler/refresh';
	import Stopwatch from '~icons/tabler/24-hours';
	import { formatDuration } from '$lib/utils';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	import type { SystemInformation } from '$lib/types/system/system';
	import type { ExtendedHealthDiagnostics } from '$lib/types/system/systemStatusSnapshot';

	// Local components
	import MemoryCard from './MemoryCard.svelte';
	import WifiHealthCard from './WifiHealthCard.svelte';
	import StorageCard from './StorageCard.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	import Activity from '~icons/tabler/activity';
	import Network from '~icons/tabler/network';

	interface Props {
		systemInfo: SystemInformation | null;
		health: ExtendedHealthDiagnostics | null;
		refreshing: boolean;
		isApMode: boolean;
		onRefresh: () => void;
		onTriggerWifiRecovery: () => void;
		onShowNetworkDiagnostics: () => void;
		onShowTasks: () => void;
	}

	let {
		systemInfo,
		health,
		refreshing,
		isApMode,
		onRefresh,
		onTriggerWifiRecovery,
		onShowNetworkDiagnostics,
		onShowTasks
	}: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';
</script>

<BaseCard
	title={m.status_health_title({ locale: i18n.languageTag })}
	icon={health?.healthy ? Heart : HeartBroken}
	iconClass="w-6 h-6 flex-none {health?.healthy ? 'text-success' : 'text-error'}"
	class="h-full"
>
	{#snippet actions()}
		<div class="flex gap-1">
			<FormButton
				label=""
				icon={Activity}
				class="btn-ghost btn-xs btn-circle"
				onclick={onShowTasks}
				aria-label={m.tooltip_task_manager({ locale: i18n.languageTag })}
				title={m.tooltip_task_manager({ locale: i18n.languageTag })}
			/>
			<FormButton
				label=""
				icon={Network}
				class="btn-ghost btn-xs btn-circle"
				onclick={onShowNetworkDiagnostics}
				aria-label={m.tooltip_network_diagnostics({ locale: i18n.languageTag })}
				title={m.tooltip_network_diagnostics({ locale: i18n.languageTag })}
			/>
			<FormButton
				label=""
				icon={Refresh}
				class="btn-ghost btn-xs btn-circle"
				onclick={onRefresh}
				disabled={refreshing}
				loading={refreshing}
				aria-label={m.tooltip_refresh({ locale: i18n.languageTag })}
				title={m.tooltip_refresh({ locale: i18n.languageTag })}
			/>
		</div>
	{/snippet}

	<div class="flex w-full flex-col gap-1">
		<!-- Uptime -->
		{#if systemInfo}
			<StatusRow
				icon={Stopwatch}
				iconClass={statIconClass}
				label={m.status_uptime({ locale: i18n.languageTag })}
				value={formatDuration(systemInfo.uptime)}
			/>
		{/if}

		{#if health}
			<StatusRow
				icon={Refresh}
				iconClass={statIconClass}
				label={m.status_maintenance_sleeps({ locale: i18n.languageTag })}
				value={health.runtime?.maintenanceSleeps ?? 0}
			/>
		{/if}

		<!-- Memory -->
		{#if systemInfo}
			<MemoryCard {systemInfo} {health} />
		{/if}

		<!-- WiFi Health -->
		{#if health}
			<WifiHealthCard {health} {isApMode} onReconnect={onTriggerWifiRecovery} />
		{/if}

		<!-- Storage (PSRAM, FS, Sketch) -->
		{#if systemInfo}
			<StorageCard {systemInfo} />
		{/if}
	</div>
</BaseCard>
