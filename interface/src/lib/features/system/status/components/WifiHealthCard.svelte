<!--
	WifiHealthCard.svelte - WiFi health status display component
	Shows WiFi connection status, RSSI, reconnects, and recovery button
-->
<script lang="ts">
	import type { ExtendedHealthDiagnostics } from '$lib/types/system/systemStatusSnapshot';

	import Wifi from '~icons/tabler/wifi';
	import WifiOff from '~icons/tabler/wifi-off';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';
	import { FormButton } from '$lib/components/shared/forms';

	interface Props {
		health: ExtendedHealthDiagnostics;
		isApMode?: boolean;
		onReconnect?: () => void;
	}

	let { health, isApMode, onReconnect }: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';
	const wifiHealthy = $derived(health.wifi.healthy);
</script>

<StatusRow label={m.status_wifi_title({ locale: i18n.languageTag })}>
	{#snippet iconSlot()}
		{#if health.wifi.connected || isApMode}
			<Wifi class="{statIconClass} {wifiHealthy || isApMode ? 'text-success' : 'text-warning'}" />
		{:else}
			<WifiOff class="{statIconClass} text-error" />
		{/if}
	{/snippet}
	{#snippet labelAddon()}
		{#if isApMode}
			<span class="badge badge-info badge-xs"
				>{m.statusbar_ap_mode({ locale: i18n.languageTag })}</span
			>
		{:else if !wifiHealthy}
			<span class="badge badge-warning badge-xs"
				>{m.status_wifi_unstable({ locale: i18n.languageTag })}</span
			>
		{/if}
	{/snippet}
	{#snippet details()}
		<div class="text-sm opacity-75">
			{#if isApMode}
				<span class="text-success">{m.status_active({ locale: i18n.languageTag })}</span>
			{:else if health.wifi.connected}
				{health.wifi.ssid || m.status_wifi_connected_default({ locale: i18n.languageTag })} • RSSI: {health
					.wifi.rssi}
				{m.unit_dbm({ locale: i18n.languageTag })}
			{:else}
				<span class="text-error">{m.status_wifi_disconnected({ locale: i18n.languageTag })}</span>
			{/if}
		</div>
		<div class="text-xs opacity-50 flex items-center gap-2">
			{m.status_wifi_reconnects({ locale: i18n.languageTag })}: {health.wifi.reconnects}
			{#if !health.wifi.connected && !isApMode && onReconnect}
				<FormButton
					label={m.action_reconnect({ locale: i18n.languageTag })}
					class="btn-xs btn-ghost text-primary"
					onclick={onReconnect}
				/>
			{/if}
		</div>
	{/snippet}
</StatusRow>
