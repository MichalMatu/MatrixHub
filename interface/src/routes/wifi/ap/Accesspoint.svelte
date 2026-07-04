<script lang="ts">
	import { onMount } from 'svelte';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import AP from '~icons/tabler/access-point';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	// Local components
	import ApStatusCard from '$lib/features/wifi/ap/ApStatusCard.svelte';
	import ApSettingsForm from '$lib/features/wifi/ap/ApSettingsForm.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	import { useApManagement } from '$lib/features/wifi/ap/useApManagement.svelte';

	const session = useSessionAccess();
	const apMgmt = useApManagement(session.apiOptions, session.canManage);

	const apState = $derived(apMgmt.state);
	let initializing = $state(true);

	onMount(() => {
		void apMgmt.loadInitialData().finally(() => {
			initializing = false;
		});
	});
</script>

{#if initializing}
	<div class="grid grid-cols-1 md:grid-cols-2 gap-3 md:gap-4">
		<LoadingCard title={m.menu_wifi_ap({ locale: i18n.languageTag })} icon={AP} loading={true} />
		{#if session.canManage}
			<LoadingCard
				title={m.ap_settings_title({ locale: i18n.languageTag })}
				icon={AP}
				loading={true}
			/>
		{/if}
	</div>
{:else}
	{#if apMgmt.statusError}
		<div class="alert alert-warning mb-4">
			<span>{m.error_prefix({ error: apMgmt.statusError }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}
	{#if apMgmt.settingsError}
		<div class="alert alert-warning mb-4">
			<span>{m.error_prefix({ error: apMgmt.settingsError }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}
	<div class="grid grid-cols-1 md:grid-cols-2 gap-3 md:gap-4">
		<!-- Left Column: AP Status -->
		<ApStatusCard status={apState.status} />

		<!-- Right Column: AP Settings -->
		{#if session.canManage}
			<ApSettingsForm
				bind:settings={apState.settings}
				loading={apState.loading}
				isDirty={apState.isSettingsDirty}
				onSave={apMgmt.saveSettings}
				onReset={apMgmt.resetSettings}
			/>
		{/if}
	</div>
{/if}
