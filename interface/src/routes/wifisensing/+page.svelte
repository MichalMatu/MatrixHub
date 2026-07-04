<script lang="ts">
	import RssiChart from '$lib/features/wifisensing/components/RssiChart.svelte';
	import Wifi from '~icons/tabler/wifi';
	import LiveStatus from '$lib/features/wifisensing/components/LiveStatus.svelte';
	import SensingSettings from '$lib/features/wifisensing/components/SensingSettings.svelte';
	import { useWifiSensingManagement } from '$lib/features/wifisensing/useWifiSensingManagement.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	const sensing = useWifiSensingManagement();

	import { PageWrapper } from '$lib/components/layout';
	import { Spinner } from '$lib/components';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
</script>

<PageWrapper>
	<!-- Error -->
	{#if sensing.error}
		<div class="alert alert-error mb-4">
			<span>{m.sensing_error({ error: sensing.error }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}

	<!-- Chart at the top -->
	{#if sensing.savedSettings}
		{#if sensing.isEnabled}
			{#if sensing.isActive && sensing.samples.length > 0}
				<RssiChart
					samples={sensing.samples}
					threshold={sensing.appliedThreshold}
					motionDetected={sensing.motionDetected}
					lastUpdate={sensing.lastUpdate}
					windowMs={sensing.sensingData?.stats?.windowMs}
				/>
			{:else}
				<BaseCard class="mb-4 h-[180px] lg:h-[220px] transition-all duration-300">
					<div class="w-full h-full flex flex-col items-center justify-center gap-3">
						<div class="opacity-60 animate-pulse flex flex-col items-center gap-3">
							<span class="loading loading-ring loading-lg text-primary scale-125"></span>
							<p class="text-xs font-mono uppercase tracking-widest text-center">
								{m.sensing_collecting(
									{ count: sensing.sensingData?.stats?.sampleCount ?? 0 },
									{ locale: i18n.languageTag }
								)}
							</p>
						</div>
					</div>
				</BaseCard>
			{/if}
		{:else}
			<BaseCard class="mb-4 h-[180px] lg:h-[220px] transition-all duration-300">
				<div class="w-full h-full flex flex-col items-center justify-center gap-3">
					<div class="flex flex-col items-center gap-2">
						<Wifi class="w-10 h-10 opacity-20 mb-1" />
						<p class="text-sm font-medium opacity-60 text-center">
							{#if sensing.hasUnsavedEnable}
								{m.sensing_save_to_start({ locale: i18n.languageTag })}
							{:else}
								{m.sensing_toggle_desc_off({ locale: i18n.languageTag })}
							{/if}
						</p>
					</div>
				</div>
			</BaseCard>
		{/if}
	{:else}
		<BaseCard class="mb-4 h-[180px] lg:h-[220px] transition-all duration-300">
			<div class="w-full h-full flex items-center justify-center">
				<Spinner />
			</div>
		</BaseCard>
	{/if}

	<div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
		{#if sensing.isAdmin}
			<div class="h-full">
				<SensingSettings
					savedSettings={sensing.savedSettings}
					bind:localEnabled={sensing.localEnabled}
					bind:localInterval={sensing.localInterval}
					bind:localThreshold={sensing.localThreshold}
					hasChanges={sensing.hasChanges}
					saving={sensing.saving}
					onSave={sensing.saveSettings}
					onReset={sensing.resetSettings}
				/>
			</div>
		{/if}

		<div class="h-full">
			<LiveStatus
				sensingData={sensing.sensingData}
				isActive={sensing.isActive}
				motionDetected={sensing.motionDetected}
				threshold={sensing.appliedThreshold}
			/>
		</div>
	</div>
</PageWrapper>
