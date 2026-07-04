<script lang="ts">
	import { FormButton } from '$lib/components/shared/forms';
	import { RSSIIndicator as RssiIndicator } from '$lib/components/indicators';
	import { appFeatures } from '$lib/stores/appFeatures.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { useStatusbarManagement } from './useStatusbarManagement.svelte';
	import WiFiOff from '~icons/tabler/wifi-off';
	import Hamburger from '~icons/tabler/menu-2';
	import Power from '~icons/tabler/power';
	import Chip from '~icons/tabler/cpu';
	import CloudCheck from '~icons/tabler/cloud-check';
	import CloudOff from '~icons/tabler/cloud-off';
	import Refresh from '~icons/tabler/refresh';
	import AlertCircle from '~icons/tabler/alert-circle';

	const statusbar = useStatusbarManagement();
	const features = $derived(appFeatures.features);
	const featuresResolved = $derived(appFeatures.resolved);
</script>

<div
	class="navbar bg-base-300 sticky top-0 z-10 min-h-12 w-full gap-2 px-2 drop-shadow-lg sm:px-3 lg:min-h-16"
>
	<div class="flex min-w-0 flex-1 items-center justify-start">
		<label
			for="main-menu"
			class="btn btn-ghost btn-circle btn-sm drawer-button shrink-0 lg:hidden"
			aria-label={m.action_open_menu()}
			title={m.action_open_menu()}
		>
			<Hamburger class="h-6 w-auto" />
		</label>
		<span
			class="inline-block min-w-0 truncate px-1 text-base font-bold sm:px-2 sm:text-xl lg:text-2xl"
			>{statusbar.currentTitle}</span
		>
	</div>
	<div
		class="hidden flex-none items-center px-1 text-sm font-mono whitespace-nowrap text-base-content/80 sm:flex"
	>
		<span class="hidden md:inline">{statusbar.currentDate}</span>
		<span class="hidden md:inline">&nbsp;</span>
		<span>{statusbar.currentClock}</span>
	</div>
	<div class="flex flex-none items-center gap-2 pl-1 sm:gap-3 sm:pr-1">
		<div
			class="tooltip tooltip-bottom flex h-8 w-8 items-center justify-center"
			data-tip={statusbar.connectionTooltip}
			role="img"
			aria-label={statusbar.connectionTooltip}
		>
			{#if statusbar.connectionStatus === 'connected'}
				<CloudCheck class={`h-5 w-5 ${statusbar.connectionClass}`} />
			{:else if statusbar.connectionStatus === 'connecting'}
				<Refresh class={`h-5 w-5 animate-spin ${statusbar.connectionClass}`} />
			{:else if statusbar.connectionStatus === 'error'}
				<AlertCircle class={`h-5 w-5 ${statusbar.connectionClass}`} />
			{:else}
				<CloudOff class={`h-5 w-5 ${statusbar.connectionClass}`} />
			{/if}
		</div>

		{#if statusbar.status.coreTemp !== undefined}
			<div class="hidden md:flex items-center text-sm font-mono text-base-content/80">
				<Chip class="h-5 w-5 mr-1 transition-colors duration-500" style={statusbar.cpuTempStyle} />
				<span>{statusbar.status.coreTemp.toFixed(1)}°C</span>
			</div>
		{/if}

		{#if statusbar.status.isConnected || statusbar.status.isApMode}
			<div class="flex items-center justify-center">
				{#if statusbar.status.isApMode}
					<div
						class="tooltip tooltip-bottom"
						data-tip={m.statusbar_ap_mode()}
						role="img"
						aria-label={m.statusbar_ap_mode()}
					>
						<RssiIndicator
							showDBm={false}
							rssi_dbm={statusbar.status.rssi < -90 ? -50 : statusbar.status.rssi}
							class="h-5 w-5 text-info"
						/>
					</div>
				{:else if statusbar.status.rssi < -90}
					<WiFiOff class="h-5 w-5 text-success" />
				{:else}
					<RssiIndicator
						showDBm={false}
						rssi_dbm={statusbar.status.rssi}
						class="h-5 w-5 text-success"
					/>
				{/if}
			</div>
		{:else}
			<div
				class="flex items-center justify-center tooltip tooltip-bottom"
				data-tip={m.status_wifi_disconnected()}
				role="img"
				aria-label={m.status_wifi_disconnected()}
			>
				<WiFiOff class="h-5 w-5 text-error" />
			</div>
		{/if}

		{#if featuresResolved && features.sleep}
			<FormButton
				label=""
				icon={Power}
				class="btn-square btn-ghost btn-sm h-8 w-8 min-h-0 p-0"
				onclick={statusbar.confirmSleep}
				ariaLabel={m.power_btn_sleep()}
				title={m.power_btn_sleep()}
			/>
		{/if}
	</div>
</div>
