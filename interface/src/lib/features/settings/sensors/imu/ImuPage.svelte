<script lang="ts">
	import Activity from '~icons/tabler/activity';
	import Bell from '~icons/tabler/bell';
	import ExternalLink from '~icons/tabler/external-link';
	import Gauge from '~icons/tabler/gauge';
	import GridDots from '~icons/tabler/grid-dots';
	import Mouse from '~icons/tabler/mouse';
	import Target from '~icons/tabler/target';
	import Refresh from '~icons/tabler/refresh';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { Spinner } from '$lib/components';
	import { FormButton, FormRange, FormToggle } from '$lib/components/shared/forms';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { useImuConsumerSettings } from './useImuConsumerSettings.svelte';
	import { useImuSettings } from './useImuSettings.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const imu = useImuSettings({ shouldLoad: () => canManage });
	const consumerSettings = useImuConsumerSettings({ shouldLoad: () => canManage });

	const status = $derived(imu.status);
	const metrics = $derived(status?.metrics ?? null);
	const alarm = $derived(status?.alarm ?? null);
	const consumerRows = $derived.by(() => {
		const consumers = status?.consumers;
		return [
			{
				key: 'ui_monitor',
				label: m.imu_consumer_ui_monitor({ locale: i18n.languageTag }),
				icon: Activity,
				configured: imu.settings.ui_monitor_enabled,
				data: consumers?.ui_monitor
			},
			{
				key: 'alarm',
				label: m.imu_consumer_alarm({ locale: i18n.languageTag }),
				icon: Bell,
				configured: imu.settings.alarm_monitor_enabled,
				data: consumers?.alarm
			},
			{
				key: 'auto_rotate',
				label: m.imu_consumer_matrix({ locale: i18n.languageTag }),
				icon: GridDots,
				configured: consumerSettings.matrixAutoRotate,
				data: consumers?.auto_rotate,
				href: '/system/matrix'
			},
			{
				key: 'airmouse_movement',
				label: m.imu_consumer_airmouse_movement({ locale: i18n.languageTag }),
				icon: Mouse,
				configured: consumerSettings.airMouseMovementEnabled,
				data: consumers?.airmouse_movement,
				href: '/usb-features/airmouse'
			},
			{
				key: 'airmouse_click',
				label: m.imu_consumer_airmouse_click({ locale: i18n.languageTag }),
				icon: Mouse,
				configured: consumerSettings.airMouseClickEnabled,
				data: consumers?.airmouse_click,
				href: '/usb-features/airmouse'
			}
		];
	});

	function yesNo(value?: boolean) {
		return value ? 'OK' : '--';
	}

	function freshLabel(value?: boolean) {
		if (value === undefined) return '--';
		return value
			? m.imu_sample_fresh({ locale: i18n.languageTag })
			: m.imu_sample_stale({ locale: i18n.languageTag });
	}

	function enabledLabel(value: boolean | null | undefined) {
		if (value === null || value === undefined)
			return m.imu_state_unknown({ locale: i18n.languageTag });
		return value
			? m.imu_state_enabled({ locale: i18n.languageTag })
			: m.imu_state_disabled({ locale: i18n.languageTag });
	}

	function desiredStateLabel(value: boolean | undefined) {
		if (value === undefined) return m.imu_state_unknown({ locale: i18n.languageTag });
		return value
			? m.imu_state_desired({ locale: i18n.languageTag })
			: m.imu_state_not_desired({ locale: i18n.languageTag });
	}

	function runningStateLabel(value: boolean | undefined) {
		if (value === undefined) return m.imu_state_unknown({ locale: i18n.languageTag });
		return value
			? m.imu_state_running({ locale: i18n.languageTag })
			: m.imu_state_idle({ locale: i18n.languageTag });
	}

	function sampleAgeLabel(value: number | null | undefined) {
		return durationSecondsLabel(value);
	}

	function compactNumber(value: number, maxDigits = 3) {
		return value.toFixed(maxDigits).replace(/\.?0+$/, '');
	}

	function durationSecondsLabel(value: number | null | undefined, maxDigits = 3) {
		if (value === null || value === undefined || Number.isNaN(value)) return '--';
		return `${compactNumber(value / 1000, maxDigits)} s`;
	}

	function numberLabel(value: number | null | undefined, digits = 2, suffix = '') {
		if (value === null || value === undefined || Number.isNaN(value)) return '--';
		return `${value.toFixed(digits)}${suffix}`;
	}

	function vectorLabel(vector: { x: number; y: number; z: number } | undefined) {
		if (!vector) return '--';
		return `${vector.x.toFixed(3)}, ${vector.y.toFixed(3)}, ${vector.z.toFixed(3)}`;
	}

	function alarmReasonLabel(reason: string | undefined) {
		switch (reason) {
			case 'tilt':
				return m.imu_alarm_reason_tilt({ locale: i18n.languageTag });
			case 'shock':
				return m.imu_alarm_reason_shock({ locale: i18n.languageTag });
			case 'stale':
				return m.imu_alarm_reason_stale({ locale: i18n.languageTag });
			case 'no_baseline':
				return m.imu_alarm_reason_no_baseline({ locale: i18n.languageTag });
			case 'unavailable':
				return m.imu_alarm_reason_unavailable({ locale: i18n.languageTag });
			case 'none':
				return m.imu_alarm_reason_none({ locale: i18n.languageTag });
			default:
				return '--';
		}
	}

	function alarmStateLabel(triggered: boolean | undefined) {
		if (triggered === undefined) return '--';
		return triggered
			? m.imu_alarm_triggered({ locale: i18n.languageTag })
			: m.imu_alarm_clear({ locale: i18n.languageTag });
	}

	function baselineValidityLabel(valid: boolean) {
		return valid
			? m.imu_baseline_valid({ locale: i18n.languageTag })
			: m.imu_baseline_missing({ locale: i18n.languageTag });
	}

	function retryAtLabel(ms: number) {
		return m.imu_retry_at({ duration: durationSecondsLabel(ms) }, { locale: i18n.languageTag });
	}

	function samplesLabel(count: number) {
		return m.imu_samples({ count: String(count) }, { locale: i18n.languageTag });
	}

	function lastStartErrorLabel(value: string | undefined) {
		if (!value) return '--';
		return value;
	}

	function enabledTextClass(value: boolean | null | undefined) {
		if (value === null || value === undefined) return 'text-base-content/70';
		return value ? 'font-semibold text-success' : 'text-base-content/70';
	}

	function stateTextClass(value: boolean | undefined) {
		if (value === undefined) return 'text-base-content/70';
		return value ? 'font-semibold text-info' : 'text-base-content/70';
	}
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<BaseCard title={m.imu_overview({ locale: i18n.languageTag })} icon={Activity} class="h-full">
			{#if imu.loading && !status}
				<div class="flex items-center justify-center py-8">
					<Spinner />
				</div>
			{:else}
				<div class="space-y-3">
					<div class="grid grid-cols-2 gap-2 xl:grid-cols-5">
						<ContentBox paddingClass="p-3">
							<div class="text-[11px] font-semibold uppercase tracking-wide opacity-60">
								{m.imu_running({ locale: i18n.languageTag })}
							</div>
							<div
								class={`mt-1 text-sm font-semibold ${status?.running ? 'text-success' : 'text-base-content/60'}`}
							>
								{yesNo(status?.running)}
							</div>
						</ContentBox>
						<ContentBox paddingClass="p-3">
							<div class="text-[11px] font-semibold uppercase tracking-wide opacity-60">
								{m.imu_sample({ locale: i18n.languageTag })}
							</div>
							<div
								class={`mt-1 text-sm font-semibold ${status?.sample_fresh ? 'text-success' : 'text-warning'}`}
							>
								{freshLabel(status?.sample_fresh)}
							</div>
							<div class="text-xs opacity-60">{sampleAgeLabel(status?.sample_age_ms)}</div>
						</ContentBox>
						<ContentBox paddingClass="p-3">
							<div class="text-[11px] font-semibold uppercase tracking-wide opacity-60">
								{m.imu_alarm_state({ locale: i18n.languageTag })}
							</div>
							<div
								class={`mt-1 text-sm font-semibold ${alarm?.triggered ? 'text-error' : 'text-success'}`}
							>
								{alarmStateLabel(alarm?.triggered)}
							</div>
							<div class="text-xs opacity-60">{alarmReasonLabel(alarm?.reason)}</div>
						</ContentBox>
						<ContentBox paddingClass="p-3">
							<div class="text-[11px] font-semibold uppercase tracking-wide opacity-60">
								{m.imu_calibration({ locale: i18n.languageTag })}
							</div>
							<div
								class={`mt-1 text-sm font-semibold ${imu.settings.orientation_baseline_valid ? 'text-success' : 'text-warning'}`}
							>
								{baselineValidityLabel(imu.settings.orientation_baseline_valid)}
							</div>
							<div class="truncate text-xs opacity-60">
								{vectorLabel(imu.settings.orientation_baseline)}
							</div>
						</ContentBox>
						<ContentBox paddingClass="p-3" class="col-span-2 xl:col-span-1">
							<div class="text-[11px] font-semibold uppercase tracking-wide opacity-60">
								{m.imu_last_error({ locale: i18n.languageTag })}
							</div>
							<div class="mt-1 truncate text-sm font-semibold">
								{lastStartErrorLabel(status?.last_start_error)}
							</div>
							{#if status?.retry_pending}
								<div class="text-xs opacity-60">{retryAtLabel(status.next_retry_ms)}</div>
							{/if}
						</ContentBox>
					</div>

					<ContentBox title={m.imu_measurements({ locale: i18n.languageTag })} paddingClass="p-3">
						<div class="grid grid-cols-2 gap-x-4 gap-y-3 text-sm md:grid-cols-3">
							<div>
								<div class="text-xs opacity-60">
									{m.imu_metric_accel({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">{numberLabel(metrics?.accel_magnitude_g, 3, ' g')}</div>
							</div>
							<div>
								<div class="text-xs opacity-60">
									{m.imu_metric_delta({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">{numberLabel(metrics?.accel_delta_g, 3, ' g')}</div>
							</div>
							<div>
								<div class="text-xs opacity-60">
									{m.imu_metric_gyro({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">{numberLabel(metrics?.gyro_magnitude_dps, 1, ' dps')}</div>
							</div>
							<div>
								<div class="text-xs opacity-60">
									{m.imu_metric_tilt({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">{numberLabel(metrics?.tilt_deg, 1, '°')}</div>
							</div>
							<div>
								<div class="text-xs opacity-60">
									{m.imu_current_accel_delta({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">{numberLabel(alarm?.accel_delta_g, 3, ' g')}</div>
							</div>
							<div>
								<div class="text-xs opacity-60">
									{m.imu_current_tilt({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">{numberLabel(alarm?.tilt_deg, 1, '°')}</div>
							</div>
							<div>
								<div class="text-xs opacity-60">
									{m.imu_alarm_trigger_hold({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">
									{durationSecondsLabel(alarm?.trigger_hold_elapsed_ms, 2)}
								</div>
							</div>
							<div>
								<div class="text-xs opacity-60">
									{m.imu_alarm_clear_hold({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">
									{durationSecondsLabel(alarm?.clear_hold_elapsed_ms, 2)}
								</div>
							</div>
							<div class="col-span-2 md:col-span-1">
								<div class="text-xs opacity-60">
									{m.imu_metric_baseline({ locale: i18n.languageTag })}
								</div>
								<div class="font-mono">{vectorLabel(metrics?.orientation_baseline)}</div>
							</div>
						</div>
					</ContentBox>

					<ContentBox title={m.imu_consumers({ locale: i18n.languageTag })} paddingClass="p-3">
						{#if consumerSettings.errorMessage}
							<div class="mb-2 text-sm text-warning">{consumerSettings.errorMessage}</div>
						{/if}
						<div class="divide-y divide-base-300/50">
							{#each consumerRows as row (row.key)}
								{@const Icon = row.icon}
								<div
									data-testid={`imu-consumer-${row.key}`}
									class="grid gap-2 py-2 first:pt-0 last:pb-0 sm:grid-cols-[minmax(0,1fr)_auto] sm:items-center"
								>
									<div class="flex min-w-0 items-center gap-3">
										<div class="flex h-8 w-8 shrink-0 items-center justify-center">
											<Icon class="h-5 w-5 text-base-content/70" />
										</div>
										<div class="min-w-0">
											<div class="truncate text-sm font-semibold">{row.label}</div>
											<div class="mt-1 flex flex-wrap gap-x-3 gap-y-1 text-xs">
												<span>
													<span class="opacity-60">
														{m.imu_consumer_configured({ locale: i18n.languageTag })}:
													</span>
													<span class={enabledTextClass(row.configured)}>
														{enabledLabel(row.configured)}
													</span>
												</span>
												<span>
													<span class="opacity-60">
														{m.imu_consumer_desired({ locale: i18n.languageTag })}:
													</span>
													<span class={stateTextClass(row.data?.desired)}>
														{desiredStateLabel(row.data?.desired)}
													</span>
												</span>
												<span>
													<span class="opacity-60">
														{m.imu_consumer_running({ locale: i18n.languageTag })}:
													</span>
													<span class={stateTextClass(row.data?.running)}>
														{runningStateLabel(row.data?.running)}
													</span>
												</span>
											</div>
										</div>
									</div>
									<div class="flex items-center justify-end gap-2">
										{#if row.key === 'auto_rotate'}
											<input
												type="checkbox"
												class="toggle toggle-primary toggle-sm"
												aria-label={m.imu_consumer_matrix({ locale: i18n.languageTag })}
												checked={consumerSettings.matrixAutoRotate === true}
												disabled={!canManage ||
													consumerSettings.matrixAutoRotate === null ||
													consumerSettings.matrixSaving}
												onchange={async (event) => {
													await consumerSettings.setMatrixAutoRotate(
														(event.target as HTMLInputElement).checked
													);
													await imu.refreshStatus();
												}}
											/>
										{/if}
										{#if row.href}
											<a class="btn btn-outline btn-xs" href={row.href} aria-label={row.label}>
												<ExternalLink class="h-3.5 w-3.5" />
												{m.imu_open_config({ locale: i18n.languageTag })}
											</a>
										{/if}
									</div>
								</div>
							{/each}
						</div>
					</ContentBox>
				</div>
			{/if}
		</BaseCard>

		<SettingsCard
			title={m.imu_alarm_orientation({ locale: i18n.languageTag })}
			icon={Gauge}
			class="h-full"
			hasChanges={imu.hasChanges}
			loading={imu.loading}
			saving={imu.saving}
			onSave={imu.saveSettings}
			onReset={imu.resetSettings}
			dirtySourceId="imu-settings"
		>
			{#if imu.loading}
				<div class="flex items-center justify-center py-8">
					<Spinner />
				</div>
			{:else}
				<form
					class="space-y-3"
					onsubmit={(event) => {
						event.preventDefault();
						imu.saveSettings();
					}}
				>
					<ContentBox title={m.imu_settings({ locale: i18n.languageTag })} paddingClass="p-3">
						<div class="space-y-2">
							<FormToggle
								label={m.imu_live_monitor({ locale: i18n.languageTag })}
								description={m.imu_live_monitor_desc({ locale: i18n.languageTag })}
								checked={imu.settings.ui_monitor_enabled}
								onchange={(event) =>
									imu.updateSetting(
										'ui_monitor_enabled',
										(event.target as HTMLInputElement).checked
									)}
							/>
							<FormToggle
								label={m.imu_alarm_monitor({ locale: i18n.languageTag })}
								description={m.imu_alarm_monitor_desc({ locale: i18n.languageTag })}
								checked={imu.settings.alarm_monitor_enabled}
								onchange={(event) =>
									imu.updateSetting(
										'alarm_monitor_enabled',
										(event.target as HTMLInputElement).checked
									)}
							/>
						</div>
					</ContentBox>

					<ContentBox title={m.imu_thresholds({ locale: i18n.languageTag })} paddingClass="p-3">
						<div class="grid gap-3">
							<FormRange
								id="imu_tilt_threshold"
								label={m.imu_tilt_threshold({ locale: i18n.languageTag })}
								value={imu.settings.tilt_threshold_deg}
								min={1}
								max={90}
								step={1}
								suffix="°"
								oninput={(event) =>
									imu.updateSetting(
										'tilt_threshold_deg',
										(event.currentTarget as HTMLInputElement).valueAsNumber
									)}
							/>
							<FormRange
								id="imu_tilt_hysteresis"
								label={m.imu_tilt_hysteresis({ locale: i18n.languageTag })}
								value={imu.settings.tilt_hysteresis_deg}
								min={0}
								max={30}
								step={1}
								suffix="°"
								oninput={(event) =>
									imu.updateSetting(
										'tilt_hysteresis_deg',
										(event.currentTarget as HTMLInputElement).valueAsNumber
									)}
							/>
							<FormRange
								id="imu_accel_delta"
								label={m.imu_accel_delta_threshold({ locale: i18n.languageTag })}
								value={imu.settings.accel_delta_threshold_g}
								min={0.01}
								max={2}
								step={0.01}
								suffix="g"
								oninput={(event) =>
									imu.updateSetting(
										'accel_delta_threshold_g',
										(event.currentTarget as HTMLInputElement).valueAsNumber
									)}
							/>
							<FormRange
								id="imu_tilt_hold"
								label={m.imu_tilt_hold({ locale: i18n.languageTag })}
								value={imu.settings.tilt_hold_ms}
								min={100}
								max={10000}
								step={50}
								valueFormatter={(value) => durationSecondsLabel(value, 2)}
								valueClass="w-16"
								oninput={(event) =>
									imu.updateSetting(
										'tilt_hold_ms',
										(event.currentTarget as HTMLInputElement).valueAsNumber
									)}
							/>
							<FormRange
								id="imu_tilt_clear_hold"
								label={m.imu_tilt_clear_hold({ locale: i18n.languageTag })}
								value={imu.settings.tilt_clear_hold_ms}
								min={100}
								max={15000}
								step={50}
								valueFormatter={(value) => durationSecondsLabel(value, 2)}
								valueClass="w-16"
								oninput={(event) =>
									imu.updateSetting(
										'tilt_clear_hold_ms',
										(event.currentTarget as HTMLInputElement).valueAsNumber
									)}
							/>
						</div>
					</ContentBox>

					<ContentBox title={m.imu_calibration({ locale: i18n.languageTag })} paddingClass="p-3">
						<div class="grid gap-3 md:grid-cols-[minmax(0,1fr)_auto] md:items-end">
							<div class="grid grid-cols-2 gap-x-4 gap-y-3 text-sm">
								<div>
									<div class="text-xs opacity-60">
										{m.imu_metric_baseline({ locale: i18n.languageTag })}
									</div>
									<div
										class={imu.settings.orientation_baseline_valid
											? 'font-semibold text-success'
											: 'font-semibold text-warning'}
									>
										{baselineValidityLabel(imu.settings.orientation_baseline_valid)}
									</div>
									<div class="font-mono text-xs opacity-70">
										{vectorLabel(imu.settings.orientation_baseline)}
									</div>
								</div>
								<div>
									<div class="text-xs opacity-60">
										{m.imu_metric_revision({ locale: i18n.languageTag })}
									</div>
									<div class="font-mono">{imu.settings.calibration_revision}</div>
									<div class="font-mono text-xs opacity-70">
										{durationSecondsLabel(imu.settings.baseline_calibrated_at, 2)}
									</div>
								</div>
							</div>
							<div class="flex justify-end gap-2">
								<FormButton
									variant="ghost"
									icon={Refresh}
									label={m.imu_reset_orientation({ locale: i18n.languageTag })}
									loading={imu.resettingBaseline}
									disabled={imu.calibrating || imu.resettingBaseline}
									onclick={imu.resetOrientationBaseline}
								/>
								<FormButton
									icon={Target}
									label={m.imu_calibrate_orientation({ locale: i18n.languageTag })}
									loading={imu.calibrating}
									disabled={imu.calibrating || imu.resettingBaseline}
									onclick={imu.calibrateOrientation}
								/>
							</div>
						</div>
						{#if imu.calibrationResult}
							<div
								class={`mt-3 rounded-md border border-base-300/60 bg-base-100/30 px-3 py-2 text-sm ${
									imu.calibrationResult.ok ? 'text-success' : 'text-warning'
								}`}
							>
								<div class="font-semibold">
									{imu.calibrationResult.ok
										? m.imu_calibration_success({ locale: i18n.languageTag })
										: m.imu_calibration_failed({ locale: i18n.languageTag })}
								</div>
								<div class="text-xs opacity-70">
									{imu.calibrationResult.status} · {samplesLabel(
										imu.calibrationResult.sample_count
									)} ·
									{numberLabel(imu.calibrationResult.accel_magnitude_variance, 4)}
								</div>
							</div>
						{/if}
					</ContentBox>
				</form>
			{/if}
		</SettingsCard>
	</GridLayout>
</AdminAccessGate>
