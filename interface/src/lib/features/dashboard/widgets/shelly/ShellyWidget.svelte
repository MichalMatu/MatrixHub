<script lang="ts">
	import Bolt from '~icons/tabler/bolt';
	import Plug from '~icons/tabler/plug';
	import Power from '~icons/tabler/power';
	import WifiOff from '~icons/tabler/wifi-off';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { FormButton } from '$lib/components/shared/forms';
	import BaseWidget from '$lib/components/layout/BaseWidget.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { useShellyWidgetManagement } from './useShellyWidgetManagement.svelte';

	const controller = useShellyWidgetManagement();
</script>

<BaseWidget
	href="/shelly"
	icon={Bolt}
	title={m.widget_shelly_title({ locale: i18n.languageTag })}
	shadowColor="warning"
	loading={controller.loading}
>
	{#snippet children()}
		{#if controller.errorMessage}
			<div class="alert alert-warning text-xs mb-2">
				<span
					>{m.error_prefix({ error: controller.errorMessage }, { locale: i18n.languageTag })}</span
				>
			</div>
		{/if}
		{#if !controller.hasDevices}
			<div class="flex flex-col items-center justify-center h-full text-base-content/50">
				<Plug class="w-8 h-8 mb-2 opacity-50" />
				<p class="text-sm">{m.widget_shelly_no_devices({ locale: i18n.languageTag })}</p>
				<p class="text-xs mt-1">{m.widget_shelly_tap_add({ locale: i18n.languageTag })}</p>
			</div>
		{:else}
			<div class="flex flex-col gap-2 overflow-y-auto">
				{#each controller.devices as device (device.id)}
					<ContentBox paddingClass="px-3 py-2" class="flex items-center justify-between gap-2">
						<!-- Device info -->
						<div class="flex items-center gap-2 min-w-0 flex-1">
							{#if device.isOnline}
								<Plug class="w-4 h-4 text-success flex-shrink-0" />
							{:else}
								<WifiOff class="w-4 h-4 text-error flex-shrink-0" />
							{/if}
							<div class="min-w-0 flex-1">
								<div class="font-medium text-sm truncate">{device.name}</div>
								<div class="text-xs text-base-content/40 flex items-center gap-2">
									<span class="truncate">{device.ip}</span>
									{#if device.isOnline && device.power !== undefined && (device.lastUpdate ?? 0) > 0}
										<span class="opacity-30">•</span>
										<span
											class="font-mono font-bold {device.power === 0
												? 'opacity-50'
												: device.power < 100
													? 'text-success'
													: device.power < 2000
														? 'text-warning'
														: 'text-error'}"
										>
											{device.power.toFixed(1)}W
										</span>
									{/if}
									{#if device.isOnline && device.temperature !== undefined && device.temperature > 0 && (device.lastUpdate ?? 0) > 0}
										<span class="opacity-30">•</span>
										<span
											class="font-mono {device.temperature < 40
												? 'text-success'
												: device.temperature < 60
													? 'text-warning'
													: 'text-error'}"
										>
											{device.temperature.toFixed(0)}°C
										</span>
									{/if}
								</div>
							</div>
						</div>

						<!-- Toggle button -->
						<FormButton
							label=""
							icon={Power}
							class="btn-circle btn-sm {device.isOn ? 'btn-primary' : 'btn-ghost'}"
							disabled={!controller.canControl ||
								!device.isOnline ||
								controller.toggling[device.id]}
							onclick={(e: MouseEvent) => controller.toggleDevice(device, e)}
							title={device.isOn
								? m.action_turn_off({ locale: i18n.languageTag })
								: m.action_turn_on({ locale: i18n.languageTag })}
							loading={controller.toggling[device.id]}
						/>
					</ContentBox>
				{/each}
			</div>
		{/if}
	{/snippet}
</BaseWidget>
