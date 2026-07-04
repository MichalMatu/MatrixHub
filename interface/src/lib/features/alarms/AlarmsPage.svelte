<script lang="ts">
	import AlarmRuleList from './components/AlarmRuleList.svelte';
	import AlarmRuleModal from './components/AlarmRuleModal.svelte';
	import AlarmInfoModal from './components/AlarmInfoModal.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { PageWrapper } from '$lib/components/layout';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import Bell from '~icons/tabler/bell';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import { useAlarmsManagement } from './useAlarmsManagement.svelte';

	const alarms = useAlarmsManagement();
</script>

<PageWrapper>
	{#if alarms.error}
		<div class="alert alert-warning mb-3">
			<AlertTriangle class="h-5 w-5" />
			<span>{m.error_prefix({ error: alarms.error }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}
	{#if alarms.loading}
		<LoadingCard title={m.alarms_title({ locale: i18n.languageTag })} icon={Bell} loading={true}>
			<!-- Content hidden while loading -->
			{#snippet children()}
				<div></div>
			{/snippet}
		</LoadingCard>
	{:else}
		<AlarmRuleList
			rules={alarms.rules}
			canManage={alarms.canManage}
			onAdd={alarms.openAddModal}
			onEdit={alarms.openEditModal}
			onDelete={alarms.confirmDelete}
			onToggle={alarms.toggleRule}
			onInfo={alarms.openInfoModal}
		/>
	{/if}

	<!-- Modal -->
	<AlarmRuleModal
		isOpen={alarms.showModal}
		rule={alarms.editingRule}
		isSaving={alarms.modalSaving}
		onSave={alarms.submitRule}
		onClose={alarms.closeRuleModal}
	/>

	<AlarmInfoModal isOpen={alarms.showInfoModal} onClose={alarms.closeInfoModal} />
</PageWrapper>
