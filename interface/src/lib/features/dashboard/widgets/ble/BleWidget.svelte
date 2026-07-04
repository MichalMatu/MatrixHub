<script lang="ts">
	import Bluetooth from '~icons/tabler/bluetooth';
	import Temperature from '~icons/tabler/temperature';
	import Droplet from '~icons/tabler/droplet';
	import Battery from '~icons/tabler/battery';
	import Antenna from '~icons/tabler/antenna';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseWidget from '$lib/components/layout/BaseWidget.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { useBleWidgetManagement } from './useBleWidgetManagement.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const controller = useBleWidgetManagement();
	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
</script>

<BaseWidget
	href="/bluetooth"
	icon={Bluetooth}
	title={m.widget_ble_title({ locale: i18n.languageTag })}
	shadowColor={controller.widgetError ? 'warning' : 'info'}
	badge={controller.hasSensors ? String(controller.sensors.length) : undefined}
	loading={controller.settingsLoading}
>
	{#snippet children()}
		{#if controller.widgetError}
			<div class="alert alert-warning py-2 text-sm">
				<AlertTriangle class="w-4 h-4" />
				<span>{controller.widgetError}</span>
			</div>
		{:else if !controller.scannerEnabled}
			<div class="flex flex-col items-center justify-center h-full text-base-content/50">
				<Bluetooth class="w-8 h-8 mb-2 opacity-50" />
				<p class="text-sm">{m.widget_ble_disabled({ locale: i18n.languageTag })}</p>
			</div>
		{:else if !controller.hasSensors}
			<div class="flex flex-col items-center justify-center h-full text-base-content/50">
				<Bluetooth class="w-8 h-8 mb-2 opacity-50" />
				<p class="text-sm">{m.widget_ble_no_sensors({ locale: i18n.languageTag })}</p>
				<p class="text-xs mt-1">
					{canManage
						? m.widget_ble_tap_add({ locale: i18n.languageTag })
						: m.widget_ble_tap_open({ locale: i18n.languageTag })}
				</p>
			</div>
		{:else}
			<div class="flex flex-col gap-2">
				{#each controller.sensors as sensor (sensor.config.mac)}
					{@const data = sensor.data}
					<ContentBox paddingClass="px-3 py-2">
						<!-- Sensor name -->
						<div class="flex items-center justify-between gap-2 mb-1 min-w-0">
							<span class="font-medium text-sm truncate flex-1"
								>{sensor.config.alias || sensor.config.mac}</span
							>
							{#if data}
								<span class="text-[10px] text-base-content/40 flex-shrink-0"
									>{controller.getTimeSince(data.lastSeen)}</span
								>
							{/if}
						</div>

						{#if data}
							<!-- Values row -->
							<div
								class="flex items-center gap-x-2 gap-y-1 text-[10px] sm:text-xs flex-wrap min-w-0"
							>
								<!-- Temperature -->
								<div class="flex items-center gap-1 flex-shrink-0">
									<Temperature class="w-3 h-3 sm:w-3.5 sm:h-3.5 text-error" />
									<span class="font-mono">{data.temp.toFixed(1)}°C</span>
								</div>
								<!-- Humidity -->
								<div class="flex items-center gap-1 flex-shrink-0">
									<Droplet class="w-3 h-3 sm:w-3.5 sm:h-3.5 text-info" />
									<span class="font-mono">{data.humid.toFixed(0)}%</span>
								</div>
								<!-- Battery -->
								<div
									class="flex items-center gap-1 flex-shrink-0 {controller.getBatteryClass(
										data.batt
									)}"
								>
									<Battery class="w-3 h-3 sm:w-3.5 sm:h-3.5" />
									<span class="font-mono">{data.batt}%</span>
								</div>
								<!-- RSSI -->
								<div
									class="flex items-center gap-1 flex-shrink-0 {controller.getRssiClass(data.rssi)}"
								>
									<Antenna class="w-3 h-3 sm:w-3.5 sm:h-3.5" />
									<span class="font-mono">{data.rssi}</span>
								</div>
							</div>
						{:else}
							<!-- No data yet -->
							<div class="flex items-center gap-2 text-xs text-base-content/40">
								<AlertTriangle class="w-3.5 h-3.5" />
								<span>{m.status_waiting({ locale: i18n.languageTag })}</span>
							</div>
						{/if}
					</ContentBox>
				{/each}
			</div>
		{/if}
	{/snippet}
</BaseWidget>
