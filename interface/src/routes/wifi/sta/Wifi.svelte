<script lang="ts">
	import { onMount } from 'svelte';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import Router from '~icons/tabler/router';
	import Settings from '~icons/tabler/settings';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	// Local components
	import WifiStatusCard from '$lib/features/wifi/sta/WifiStatusCard.svelte';
	import ConnectionSettings from '$lib/features/wifi/sta/ConnectionSettings.svelte';
	import NetworkListCard from '$lib/features/wifi/sta/NetworkListCard.svelte';
	import { useWifiManagement } from '$lib/features/wifi/sta/useWifiManagement.svelte';

	const session = useSessionAccess();
	const wifiMgmt = useWifiManagement(session.apiOptions, session.canManage);

	const state = $derived(wifiMgmt.state);

	onMount(() => {
		void wifiMgmt.loadInitialData();
	});
</script>

<div class="grid grid-cols-1 md:grid-cols-2 gap-3 md:gap-4">
	{#if wifiMgmt.statusError}
		<div class="md:col-span-2 alert alert-error">
			<span>{m.error_prefix({ error: wifiMgmt.statusError }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}
	{#if wifiMgmt.settingsError}
		<div class="md:col-span-2 alert alert-error">
			<span>{m.error_prefix({ error: wifiMgmt.settingsError }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}

	<div class="contents md:flex md:flex-col md:gap-4">
		<div>
			{#if state.statusLoading}
				<LoadingCard
					title={m.status_wifi_title({ locale: i18n.languageTag })}
					icon={Router}
					loading={true}
				/>
			{:else}
				<WifiStatusCard status={state.status} />
			{/if}
		</div>

		{#if session.canManage}
			<div>
				{#if state.settingsLoading}
					<LoadingCard
						title={m.wifi_connection_title({ locale: i18n.languageTag })}
						icon={Settings}
						loading={true}
					/>
				{:else}
					<ConnectionSettings
						hostname={state.settings.hostname}
						mode={state.settings.mode}
						isDirty={state.isSettingsDirty}
						saveBlocked={state.isSaveBlocked}
						onApply={wifiMgmt.saveSettings}
						onReset={wifiMgmt.resetSettings}
						onHostnameChange={wifiMgmt.updateHostname}
						onModeChange={wifiMgmt.updateMode}
					/>
				{/if}
			</div>
		{/if}
	</div>

	{#if session.canManage}
		<div>
			{#if state.settingsLoading}
				<LoadingCard
					title={m.wifi_sta_saved_networks({ locale: i18n.languageTag })}
					icon={Router}
					loading={true}
				/>
			{:else}
				<NetworkListCard
					networks={state.settings.wifi_networks}
					onNetworksChange={wifiMgmt.updateNetworks}
					isDirty={state.isSettingsDirty}
					saveBlocked={state.isSaveBlocked}
					onApply={wifiMgmt.saveSettings}
					onReset={wifiMgmt.resetSettings}
				/>
			{/if}
		</div>
	{/if}
</div>
