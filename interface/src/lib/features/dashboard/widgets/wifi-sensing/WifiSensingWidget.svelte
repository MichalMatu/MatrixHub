<script lang="ts">
	import Wifi from '~icons/tabler/wifi';
	import Activity from '~icons/tabler/activity';
	import Walk from '~icons/tabler/walk';
	import Armchair from '~icons/tabler/armchair';
	import Clock from '~icons/tabler/clock';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseWidget from '$lib/components/layout/BaseWidget.svelte';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import { useWifiSensingWidgetManagement } from './useWifiSensingWidgetManagement.svelte';

	const controller = useWifiSensingWidgetManagement();
</script>

<BaseWidget
	href="/wifisensing"
	icon={Wifi}
	title={m.widget_wifisensing_title({ locale: i18n.languageTag })}
	shadowColor={controller.errorMessage ? 'warning' : 'primary'}
	badge={controller.enabled && controller.hasData ? `${controller.rssi}dBm` : undefined}
	loading={controller.loading}
>
	{#snippet children()}
		<div class="flex flex-col h-full min-h-0">
			{#if controller.errorMessage}
				<div class="alert alert-warning py-2 text-sm mb-2 flex-shrink-0">
					<AlertTriangle class="w-4 h-4" />
					<span>{controller.errorMessage}</span>
				</div>
			{/if}
			{#if !controller.enabled}
				<div class="flex flex-col items-center justify-center flex-1 min-h-0 text-base-content/50">
					<Wifi class="w-8 h-8 mb-2 opacity-30" />
					<p class="text-sm">{m.widget_wifisensing_disabled({ locale: i18n.languageTag })}</p>
				</div>
			{:else if !controller.hasData}
				<div class="flex flex-col items-center justify-center flex-1 min-h-0 text-base-content/50">
					<Wifi class="w-8 h-8 mb-2 opacity-30" />
					<p class="text-sm">{m.status_waiting({ locale: i18n.languageTag })}</p>
				</div>
			{:else}
				<div class="flex flex-col gap-3 overflow-y-auto flex-1 min-h-0">
					<!-- Main Status -->
					<div class="flex items-center gap-3">
						<div
							class="relative w-12 h-12 flex-shrink-0 flex items-center justify-center rounded-full bg-base-100 border-2"
							class:border-error={controller.motionDetected}
							class:border-success={!controller.motionDetected}
							class:animate-pulse={controller.motionDetected}
						>
							{#if controller.motionDetected}
								<Walk class="w-6 h-6 text-error" />
							{:else}
								<Armchair class="w-6 h-6 text-success" />
							{/if}
						</div>

						<div class="flex flex-col min-w-0 flex-1">
							<span
								class="font-bold text-lg leading-tight truncate"
								class:text-error={controller.motionDetected}
								class:text-success={!controller.motionDetected}
							>
								{controller.motionDetected
									? m.widget_wifisensing_motion_detected({ locale: i18n.languageTag })
									: m.widget_wifisensing_no_presence({ locale: i18n.languageTag })}
							</span>
							<div
								class="flex items-center gap-x-2 gap-y-1 text-[10px] text-base-content/60 flex-wrap min-w-0"
							>
								<div class="flex items-center gap-1 truncate">
									<Activity class="w-3 h-3 flex-shrink-0" />
									<span class="truncate"
										>{m.widget_wifisensing_variance({ locale: i18n.languageTag })}:
										<span
											class="font-mono font-bold {controller.getVarianceColor(
												controller.variance,
												controller.varianceThreshold
											)}">{controller.variance.toFixed(2)}</span
										><span class="opacity-50">/{controller.varianceThreshold.toFixed(1)}</span
										></span
									>
								</div>
								{#if controller.lastUpdateTime}
									<span class="opacity-30">•</span>
									<div class="flex items-center gap-1 opacity-60 flex-shrink-0">
										<Clock class="w-3 h-3" />
										<span class="font-mono">{controller.formatTime(controller.lastUpdateTime)}</span
										>
									</div>
								{/if}
							</div>
						</div>
					</div>

					<!-- Mini visualizer bar -->
					<div class="w-full bg-base-100 rounded-full h-1.5 overflow-hidden flex">
						<!-- Scale relative to threshold for better visuals -->
						<div
							class="h-full transition-all duration-300 ease-out"
							class:bg-success={controller.variance < controller.varianceThreshold * 0.6}
							class:bg-warning={controller.variance >= controller.varianceThreshold * 0.6 &&
								controller.variance < controller.varianceThreshold}
							class:bg-error={controller.variance >= controller.varianceThreshold}
							style="width: {Math.min(
								100,
								(controller.variance / (controller.varianceThreshold * 2)) * 100
							)}%"
						></div>
					</div>
				</div>
			{/if}
		</div>
	{/snippet}
</BaseWidget>
