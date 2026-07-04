<script lang="ts">
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import NtpStatusCard from './components/NtpStatusCard.svelte';
	import NtpSettingsForm from './components/NtpSettingsForm.svelte';
	import { useNtpManagement } from './useNtpManagement.svelte';
	import Clock from '~icons/tabler/clock';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	const session = useSessionAccess();
	const ntp = useNtpManagement({ shouldLoad: () => true });
</script>

<div class="grid grid-cols-1 md:grid-cols-2 gap-3 md:gap-4">
	<!-- Left Column: Time Status -->
	<div class="flex flex-col gap-3">
		{#if ntp.statusError}
			<div class="alert alert-error">
				<span>{m.error_prefix({ error: ntp.statusError }, { locale: i18n.languageTag })}</span>
			</div>
		{/if}
		{#if ntp.status}
			<NtpStatusCard
				status={ntp.status}
				settings={ntp.settingsLoaded ? ntp.settings : null}
				onRefresh={ntp.refreshStatus}
			/>
		{:else if !ntp.statusError}
			<LoadingCard
				title={m.ntp_status_title({ locale: i18n.languageTag })}
				icon={Clock}
				loading={true}
			/>
		{/if}
	</div>

	<!-- Right Column: Time Settings -->
	{#if session.canManage}
		<div class="flex flex-col gap-3">
			{#if ntp.settingsError}
				<div class="alert alert-error">
					<span>{m.error_prefix({ error: ntp.settingsError }, { locale: i18n.languageTag })}</span>
				</div>
			{/if}
			{#if ntp.settingsLoaded}
				<NtpSettingsForm
					bind:settings={ntp.settings}
					bind:manualTimeInput={ntp.manualTimeInput}
					onSubmit={ntp.saveSettings}
					onReset={ntp.resetSettings}
					isDirty={ntp.isDirty}
				/>
			{:else if !ntp.settingsError}
				<LoadingCard
					title={m.ntp_settings_title({ locale: i18n.languageTag })}
					icon={Clock}
					loading={true}
				/>
			{/if}
		</div>
	{/if}
</div>
