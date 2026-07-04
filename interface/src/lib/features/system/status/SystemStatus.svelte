<script lang="ts">
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	// Local components
	import {
		HealthCard,
		HardwareInfoCard,
		NetworkDiagnosticsModal,
		TaskManagerModal
	} from './components';
	import { useSystemStatusManagement } from './useSystemStatusManagement.svelte';

	import AlertTriangle from '~icons/tabler/alert-triangle';
	import Heart from '~icons/tabler/heart';
	import CPU from '~icons/tabler/cpu';
	let tasksModalOpen = $state(false);
	let networkModalOpen = $state(false);
	const statusView = useSystemStatusManagement();
</script>

{#if statusView.loading}
	<div class="grid grid-cols-1 md:grid-cols-2 gap-3 md:gap-4">
		<LoadingCard
			title={m.status_health_title({ locale: i18n.languageTag })}
			icon={Heart}
			loading={true}
		/>
		<LoadingCard
			title={m.status_hw_title({ locale: i18n.languageTag })}
			icon={CPU}
			loading={true}
		/>
	</div>
{:else if statusView.error && !statusView.systemInfo}
	<div class="alert alert-error">
		<AlertTriangle class="h-5 w-5" />
		<span>{m.status_fetch_error({ locale: i18n.languageTag })}: {statusView.error}</span>
	</div>
{:else}
	<div class="space-y-3">
		{#if statusView.error}
			<div class="alert alert-warning mb-3">
				<AlertTriangle class="h-5 w-5" />
				<span>{m.status_fetch_error({ locale: i18n.languageTag })}: {statusView.error}</span>
			</div>
		{/if}
		<div class="grid grid-cols-1 md:grid-cols-2 gap-3 md:gap-4">
			<!-- Left Column: System Health -->
			<HealthCard
				systemInfo={statusView.systemInfo}
				health={statusView.health}
				refreshing={statusView.refreshing}
				isApMode={statusView.isApMode}
				onRefresh={statusView.fetchAll}
				onTriggerWifiRecovery={statusView.triggerWifiRecovery}
				onShowNetworkDiagnostics={() => (networkModalOpen = true)}
				onShowTasks={() => (tasksModalOpen = true)}
			/>

			<!-- Right Column: Hardware / Firmware -->
			{#if statusView.systemInfo}
				<HardwareInfoCard systemInfo={statusView.systemInfo} />
			{/if}
		</div>

		<NetworkDiagnosticsModal bind:open={networkModalOpen} health={statusView.health} />
		<TaskManagerModal bind:open={tasksModalOpen} />
	</div>
{/if}
