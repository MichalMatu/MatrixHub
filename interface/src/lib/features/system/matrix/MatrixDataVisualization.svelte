<script lang="ts">
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import {
		FormButton,
		FormColorInput,
		FormInput,
		FormRange,
		FormSelect
	} from '$lib/components/shared/forms';
	import { Spinner } from '$lib/components/common';
	import { BleApiService } from '$lib/services/api/connectivity/BleApiService';
	import {
		MatrixApiService,
		type MatrixDataVisualizationStatus
	} from '$lib/services/api/core/MatrixApiService';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import type { BleStatus } from '$lib/types/connectivity/ble';
	import ChartBar from '~icons/tabler/chart-bar';
	import Refresh from '~icons/tabler/refresh';
	import { type useMatrixSettings } from './useMatrixSettings.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import {
		MATRIX_BACKGROUND_MODE_DATA_VISUALIZATION,
		MATRIX_BACKGROUND_MODE_EFFECTS,
		MATRIX_BACKGROUND_MODE_OFF,
		MATRIX_DATA_METRIC_CO2,
		MATRIX_DATA_METRIC_CSI_MOTION,
		MATRIX_DATA_METRIC_HUMIDITY,
		MATRIX_DATA_METRIC_RSSI,
		MATRIX_DATA_METRIC_SIGNAL_QUALITY,
		MATRIX_DATA_METRIC_TEMPERATURE,
		MATRIX_DATA_SOURCE_BLE,
		MATRIX_DATA_SOURCE_SCD4X,
		MATRIX_DATA_SOURCE_WIFI_CSI,
		MATRIX_DATA_SOURCE_WIFI_RSSI,
		MATRIX_DATA_STALE_BLANK,
		MATRIX_DATA_STALE_DIM,
		MATRIX_DATA_STALE_GRAY,
		MATRIX_DATA_VIZ_MODE_GAUGE,
		MATRIX_DATA_VIZ_MODE_HEATMAP,
		MATRIX_DATA_VIZ_MODE_PERIMETER_METER,
		MATRIX_DATA_VIZ_MODE_PULSE,
		MATRIX_DATA_VIZ_MODE_SPECTRUM_BARS,
		MATRIX_DATA_VIZ_MODE_TREND,
		fromMatrixHexColor,
		getAllowedMatrixDataVisualizationModes,
		getDefaultMatrixDataVisualizationMetric,
		getMatrixDataVisualizationPreset,
		isMatrixDataVisualizationModeValidForSource,
		toMatrixHexColor,
		type MatrixBackgroundMode,
		type MatrixDataVisualizationMetric,
		type MatrixDataVisualizationMode,
		type MatrixDataVisualizationSource,
		type MatrixDataVisualizationStaleBehavior
	} from './matrixModel';

	type SelectOption<T extends number | string = number> = {
		value: T;
		label: string;
	};

	let {
		store,
		canManage = true
	}: {
		store: ReturnType<typeof useMatrixSettings>;
		canManage?: boolean;
	} = $props();

	const session = useSessionAccess();
	let bleStatus = $state<BleStatus | null>(null);
	let bleLoadError = $state('');
	let dataStatus = $state<MatrixDataVisualizationStatus | null>(null);
	let dataStatusError = $state('');
	let minHex = $state('#00ff80');
	let midHex = $state('#ffd166');
	let maxHex = $state('#ff3000');

	const dataVisualizationActive = $derived(
		store.settings.background_mode === MATRIX_BACKGROUND_MODE_DATA_VISUALIZATION &&
			store.settings.data_visualization_enabled
	);
	const controlsDisabled = $derived(!canManage || !dataVisualizationActive);
	const selectedSource = $derived(
		store.settings.data_visualization_source as MatrixDataVisualizationSource
	);
	const showBleSelector = $derived(selectedSource === MATRIX_DATA_SOURCE_BLE);

	$effect(() => {
		if (!store.loading) {
			minHex = toMatrixHexColor(store.settings.data_visualization_color_min);
			midHex = toMatrixHexColor(store.settings.data_visualization_color_mid);
			maxHex = toMatrixHexColor(store.settings.data_visualization_color_max);
		}
	});

	$effect(() => {
		if (!session.canRead || !dataVisualizationActive) {
			dataStatus = null;
			return;
		}

		void loadDataVisualizationStatus();
		const interval = window.setInterval(() => void loadDataVisualizationStatus(), 2500);
		return () => window.clearInterval(interval);
	});

	$effect(() => {
		if (!session.canRead || !dataVisualizationActive || selectedSource !== MATRIX_DATA_SOURCE_BLE) {
			return;
		}

		void loadBleStatus();
		const interval = window.setInterval(() => void loadBleStatus(), 15000);
		return () => window.clearInterval(interval);
	});

	const sourceOptions = $derived([
		{ value: MATRIX_DATA_SOURCE_SCD4X, label: m.matrix_data_viz_source_scd4x() },
		{ value: MATRIX_DATA_SOURCE_BLE, label: m.matrix_data_viz_source_ble() },
		{ value: MATRIX_DATA_SOURCE_WIFI_RSSI, label: m.matrix_data_viz_source_wifi_rssi() },
		{ value: MATRIX_DATA_SOURCE_WIFI_CSI, label: m.matrix_data_viz_source_wifi_csi() }
	]);

	const backgroundModeOptions = $derived([
		{ value: MATRIX_BACKGROUND_MODE_OFF, label: m.matrix_background_mode_off() },
		{ value: MATRIX_BACKGROUND_MODE_EFFECTS, label: m.matrix_background_mode_effects() },
		{
			value: MATRIX_BACKGROUND_MODE_DATA_VISUALIZATION,
			label: m.matrix_background_mode_live_data()
		}
	]);

	const allModeOptions: SelectOption<MatrixDataVisualizationMode>[] = $derived([
		{ value: MATRIX_DATA_VIZ_MODE_GAUGE, label: m.matrix_data_viz_mode_gauge() },
		{ value: MATRIX_DATA_VIZ_MODE_HEATMAP, label: m.matrix_data_viz_mode_heatmap() },
		{ value: MATRIX_DATA_VIZ_MODE_TREND, label: m.matrix_data_viz_mode_trend() },
		{ value: MATRIX_DATA_VIZ_MODE_SPECTRUM_BARS, label: m.matrix_data_viz_mode_spectrum_bars() },
		{
			value: MATRIX_DATA_VIZ_MODE_PERIMETER_METER,
			label: m.matrix_data_viz_mode_perimeter_meter()
		},
		{ value: MATRIX_DATA_VIZ_MODE_PULSE, label: m.matrix_data_viz_mode_pulse() }
	]);

	const modeOptions = $derived.by(() => {
		const allowed = getAllowedMatrixDataVisualizationModes(selectedSource);
		return allModeOptions.filter((option) => allowed.includes(option.value));
	});

	const staleOptions = $derived([
		{ value: MATRIX_DATA_STALE_DIM, label: m.matrix_data_viz_stale_dim() },
		{ value: MATRIX_DATA_STALE_GRAY, label: m.matrix_data_viz_stale_gray() },
		{ value: MATRIX_DATA_STALE_BLANK, label: m.matrix_data_viz_stale_blank() }
	]);

	const metricOptions = $derived.by(() => {
		switch (selectedSource) {
			case MATRIX_DATA_SOURCE_BLE:
				return [
					{ value: MATRIX_DATA_METRIC_TEMPERATURE, label: m.matrix_data_viz_metric_temperature() },
					{ value: MATRIX_DATA_METRIC_HUMIDITY, label: m.matrix_data_viz_metric_humidity() },
					{ value: MATRIX_DATA_METRIC_RSSI, label: m.matrix_data_viz_metric_rssi() }
				];
			case MATRIX_DATA_SOURCE_WIFI_RSSI:
				return [
					{ value: MATRIX_DATA_METRIC_RSSI, label: m.matrix_data_viz_metric_rssi() },
					{
						value: MATRIX_DATA_METRIC_SIGNAL_QUALITY,
						label: m.matrix_data_viz_metric_signal_quality()
					}
				];
			case MATRIX_DATA_SOURCE_WIFI_CSI:
				return [
					{ value: MATRIX_DATA_METRIC_CSI_MOTION, label: m.matrix_data_viz_metric_csi_motion() }
				];
			case MATRIX_DATA_SOURCE_SCD4X:
			default:
				return [
					{ value: MATRIX_DATA_METRIC_CO2, label: m.matrix_data_viz_metric_co2() },
					{ value: MATRIX_DATA_METRIC_TEMPERATURE, label: m.matrix_data_viz_metric_temperature() },
					{ value: MATRIX_DATA_METRIC_HUMIDITY, label: m.matrix_data_viz_metric_humidity() }
				];
		}
	});

	$effect(() => {
		if (store.loading) return;
		const currentMode = store.settings.data_visualization_mode as MatrixDataVisualizationMode;
		if (!isMatrixDataVisualizationModeValidForSource(selectedSource, currentMode)) {
			store.settings.data_visualization_mode =
				getAllowedMatrixDataVisualizationModes(selectedSource)[0];
		}
	});

	const bleDeviceOptions = $derived.by(() => [
		{ value: '', label: m.matrix_data_viz_ble_device_placeholder() },
		...(bleStatus?.devices ?? []).map((device) => ({
			value: device.mac,
			label: `${device.mac} - ${device.temp.toFixed(1)}C - ${device.humid.toFixed(0)}% - ${device.rssi} dBm - ${device.batt}% - ${formatBleLastSeen(device.last_seen)}`
		}))
	]);

	function applyPreset(
		source: MatrixDataVisualizationSource,
		metric: MatrixDataVisualizationMetric
	) {
		const preset = getMatrixDataVisualizationPreset(source, metric);
		Object.assign(store.settings, preset);
		minHex = toMatrixHexColor(preset.data_visualization_color_min);
		midHex = toMatrixHexColor(preset.data_visualization_color_mid);
		maxHex = toMatrixHexColor(preset.data_visualization_color_max);
	}

	function setDefaultMetricForSource(source: MatrixDataVisualizationSource) {
		const metric = getDefaultMatrixDataVisualizationMetric(source);
		store.settings.data_visualization_metric = metric;
		applyPreset(source, metric);
	}

	function handleBackgroundModeChange(e: Event) {
		if (!canManage) return;
		const mode = Number((e.currentTarget as HTMLSelectElement).value) as MatrixBackgroundMode;
		store.settings.background_mode = mode;

		if (mode === MATRIX_BACKGROUND_MODE_DATA_VISUALIZATION) {
			store.settings.data_visualization_enabled = true;
			store.settings.effect_enabled = false;
		} else if (mode === MATRIX_BACKGROUND_MODE_EFFECTS) {
			store.settings.effect_enabled = true;
			store.settings.data_visualization_enabled = false;
		} else {
			store.settings.effect_enabled = false;
			store.settings.data_visualization_enabled = false;
		}
	}

	function handleSourceChange(e: Event) {
		if (!canManage) return;
		const source = Number(
			(e.currentTarget as HTMLSelectElement).value
		) as MatrixDataVisualizationSource;
		store.settings.data_visualization_source = source;
		store.settings.data_visualization_enabled = true;
		store.settings.background_mode = MATRIX_BACKGROUND_MODE_DATA_VISUALIZATION;
		store.settings.effect_enabled = false;
		setDefaultMetricForSource(source);
	}

	function handleMetricChange(e: Event) {
		if (!canManage) return;
		const metric = Number(
			(e.currentTarget as HTMLSelectElement).value
		) as MatrixDataVisualizationMetric;
		store.settings.data_visualization_metric = metric;
		applyPreset(selectedSource, metric);
	}

	function handleModeChange(e: Event) {
		if (!canManage) return;
		store.settings.data_visualization_mode = Number(
			(e.currentTarget as HTMLSelectElement).value
		) as MatrixDataVisualizationMode;
	}

	function handleStaleChange(e: Event) {
		if (!canManage) return;
		store.settings.data_visualization_stale_behavior = Number(
			(e.currentTarget as HTMLSelectElement).value
		) as MatrixDataVisualizationStaleBehavior;
	}

	function handleColorMin(e: Event) {
		if (!canManage) return;
		minHex = (e.target as HTMLInputElement).value;
		store.settings.data_visualization_color_min = fromMatrixHexColor(minHex);
	}

	function handleColorMid(e: Event) {
		if (!canManage) return;
		midHex = (e.target as HTMLInputElement).value;
		store.settings.data_visualization_color_mid = fromMatrixHexColor(midHex);
	}

	function handleColorMax(e: Event) {
		if (!canManage) return;
		maxHex = (e.target as HTMLInputElement).value;
		store.settings.data_visualization_color_max = fromMatrixHexColor(maxHex);
	}

	async function loadBleStatus() {
		try {
			bleLoadError = '';
			bleStatus = await new BleApiService(session.apiOptions).getStatus();
		} catch {
			bleLoadError = m.matrix_data_viz_ble_load_error({ locale: i18n.languageTag });
		}
	}

	async function loadDataVisualizationStatus() {
		try {
			dataStatusError = '';
			dataStatus = await new MatrixApiService(session.apiOptions).getDataVisualizationStatus();
		} catch {
			dataStatusError = m.matrix_data_viz_status_load_error({ locale: i18n.languageTag });
		}
	}

	function formatBleLastSeen(lastSeen: number): string {
		if (!lastSeen) return m.matrix_data_viz_ble_last_seen_unknown({ locale: i18n.languageTag });
		const ageMs = Math.max(0, Date.now() - lastSeen);
		const ageSeconds = Math.round(ageMs / 1000);
		return m.matrix_data_viz_ble_last_seen_age(
			{ seconds: ageSeconds },
			{ locale: i18n.languageTag }
		);
	}

	function formatStatusValue(status: MatrixDataVisualizationStatus | null): string {
		if (!status || !status.valid) return '-';
		return Number.isFinite(status.value) ? status.value.toFixed(1) : '-';
	}

	function formatStatusAge(status: MatrixDataVisualizationStatus | null): string {
		if (!status || status.last_update_ms === 0) return '-';
		return `${status.age_ms} ms`;
	}

	function getStatusReasonLabel(reason: string): string {
		const labels: Record<string, string> = {
			ok: m.matrix_data_viz_reason_ok({ locale: i18n.languageTag }),
			disabled: m.matrix_data_viz_reason_disabled({ locale: i18n.languageTag }),
			no_service: m.matrix_data_viz_reason_no_service({ locale: i18n.languageTag }),
			no_sample: m.matrix_data_viz_reason_no_sample({ locale: i18n.languageTag }),
			stale: m.matrix_data_viz_reason_stale({ locale: i18n.languageTag }),
			wifi_inactive: m.matrix_data_viz_reason_wifi_inactive({ locale: i18n.languageTag }),
			no_ble_device: m.matrix_data_viz_reason_no_ble_device({ locale: i18n.languageTag }),
			no_scd4x_reading: m.matrix_data_viz_reason_no_scd4x_reading({ locale: i18n.languageTag }),
			no_csi_packet: m.matrix_data_viz_reason_no_csi_packet({ locale: i18n.languageTag })
		};
		return labels[reason] ?? reason;
	}
</script>

<SettingsCard
	title={m.matrix_data_viz_title()}
	icon={ChartBar}
	hasChanges={store.hasChanges}
	loading={store.loading}
	saving={store.saving}
	disabled={!canManage}
	error={store.error}
	onSave={store.saveSettingsNow}
	onReset={store.resetSettings}
	dirtySourceId="matrix-data-visualization"
>
	<div class="flex w-full flex-col gap-1">
		{#if store.loading}
			<div class="flex justify-center items-center py-8">
				<Spinner />
			</div>
		{:else if store.error}
			<div class="text-error text-sm p-2 bg-error/10 rounded">
				{store.error}
			</div>
		{:else}
			<ContentBox>
				<FormSelect
					label={m.matrix_background_mode()}
					value={store.settings.background_mode}
					options={backgroundModeOptions}
					disabled={!canManage}
					onchange={handleBackgroundModeChange}
				/>
				<p class="mt-2 text-xs text-base-content/70">{m.matrix_data_viz_desc()}</p>
			</ContentBox>

			<ContentBox>
				<div class="flex items-center justify-between gap-3">
					<span class="font-bold text-sm">{m.matrix_data_viz_status_title()}</span>
					<span
						class="badge badge-sm {dataStatus?.valid && !dataStatus?.stale
							? 'badge-success'
							: 'badge-warning'}"
					>
						{dataStatus?.valid && !dataStatus?.stale
							? m.matrix_data_viz_status_live({ locale: i18n.languageTag })
							: m.matrix_data_viz_status_no_fresh_data({ locale: i18n.languageTag })}
					</span>
				</div>
				<p class="mt-2 text-xs text-base-content/70">
					{m.matrix_data_viz_status_summary(
						{
							value: formatStatusValue(dataStatus),
							age: formatStatusAge(dataStatus),
							bins: dataStatus?.bin_count ?? 0,
							reason: getStatusReasonLabel(dataStatus?.reason ?? 'disabled')
						},
						{ locale: i18n.languageTag }
					)}
				</p>
				{#if dataStatusError}
					<p class="mt-2 text-xs text-warning">{dataStatusError}</p>
				{/if}
			</ContentBox>

			<div class="flex flex-col gap-1" aria-disabled={controlsDisabled}>
				<ContentBox>
					<FormSelect
						label={m.matrix_data_viz_source()}
						value={store.settings.data_visualization_source}
						options={sourceOptions}
						disabled={controlsDisabled}
						onchange={handleSourceChange}
					/>
				</ContentBox>

				<ContentBox>
					<FormSelect
						label={m.matrix_data_viz_metric()}
						value={store.settings.data_visualization_metric}
						options={metricOptions}
						disabled={controlsDisabled}
						onchange={handleMetricChange}
					/>
				</ContentBox>

				{#if showBleSelector}
					<ContentBox>
						<div class="flex items-end gap-2">
							<div class="min-w-0 flex-1">
								<FormSelect
									label={m.matrix_data_viz_ble_device()}
									value={store.settings.data_visualization_device_id}
									options={bleDeviceOptions}
									disabled={controlsDisabled}
									onchange={(e) =>
										(store.settings.data_visualization_device_id = String(
											(e.currentTarget as HTMLSelectElement).value
										))}
								/>
							</div>
							<FormButton
								variant="ghost"
								size="sm"
								icon={Refresh}
								disabled={!canManage}
								ariaLabel={m.tooltip_refresh({ locale: i18n.languageTag })}
								onclick={() => void loadBleStatus()}
							/>
						</div>
						{#if bleLoadError}
							<p class="mt-2 text-xs text-warning">{bleLoadError}</p>
						{/if}
					</ContentBox>
				{/if}

				<ContentBox>
					<FormSelect
						label={m.matrix_data_viz_mode()}
						value={store.settings.data_visualization_mode}
						options={modeOptions}
						disabled={controlsDisabled}
						onchange={handleModeChange}
					/>
				</ContentBox>

				<ContentBox>
					<div class="grid gap-3 sm:grid-cols-2">
						<FormInput
							type="number"
							step="0.1"
							label={m.matrix_data_viz_min()}
							bind:value={store.settings.data_visualization_min}
							disabled={controlsDisabled}
						/>
						<FormInput
							type="number"
							step="0.1"
							label={m.matrix_data_viz_max()}
							bind:value={store.settings.data_visualization_max}
							disabled={controlsDisabled}
						/>
					</div>
				</ContentBox>

				<ContentBox>
					<div class="flex items-start justify-between gap-3">
						<span class="font-bold text-sm">{m.matrix_data_viz_colors()}</span>
						<div class="flex min-w-0 flex-col gap-2">
							<div class="grid grid-cols-[minmax(6rem,1fr)_auto_auto] items-center gap-2">
								<span class="truncate text-xs text-base-content/80">
									{m.matrix_data_viz_color_low()}
								</span>
								<span class="font-mono text-xs opacity-70">{minHex}</span>
								<FormColorInput
									value={minHex}
									disabled={controlsDisabled}
									ariaLabel={m.matrix_data_viz_color_min({ locale: i18n.languageTag })}
									oninput={handleColorMin}
								/>
							</div>
							<div class="grid grid-cols-[minmax(6rem,1fr)_auto_auto] items-center gap-2">
								<span class="truncate text-xs text-base-content/80">
									{m.matrix_data_viz_color_mid_label()}
								</span>
								<span class="font-mono text-xs opacity-70">{midHex}</span>
								<FormColorInput
									value={midHex}
									disabled={controlsDisabled}
									ariaLabel={m.matrix_data_viz_color_mid({ locale: i18n.languageTag })}
									oninput={handleColorMid}
								/>
							</div>
							<div class="grid grid-cols-[minmax(6rem,1fr)_auto_auto] items-center gap-2">
								<span class="truncate text-xs text-base-content/80">
									{m.matrix_data_viz_color_high()}
								</span>
								<span class="font-mono text-xs opacity-70">{maxHex}</span>
								<FormColorInput
									value={maxHex}
									disabled={controlsDisabled}
									ariaLabel={m.matrix_data_viz_color_max({ locale: i18n.languageTag })}
									oninput={handleColorMax}
								/>
							</div>
						</div>
					</div>
				</ContentBox>

				<ContentBox>
					<FormRange
						label={m.matrix_data_viz_brightness_min()}
						min={0}
						max={255}
						step={1}
						valueClass="w-12"
						disabled={controlsDisabled}
						bind:value={store.settings.data_visualization_brightness_min}
					/>
					<FormRange
						class="mt-3"
						label={m.matrix_data_viz_brightness_max()}
						min={0}
						max={255}
						step={1}
						valueClass="w-12"
						disabled={controlsDisabled}
						bind:value={store.settings.data_visualization_brightness_max}
					/>
				</ContentBox>

				<ContentBox>
					<FormRange
						label={m.matrix_data_viz_smoothing()}
						min={0}
						max={100}
						step={1}
						valueClass="w-12"
						disabled={controlsDisabled}
						bind:value={store.settings.data_visualization_smoothing}
					/>
				</ContentBox>

				<ContentBox>
					<FormSelect
						label={m.matrix_data_viz_stale_behavior()}
						value={store.settings.data_visualization_stale_behavior}
						options={staleOptions}
						disabled={controlsDisabled}
						onchange={handleStaleChange}
					/>
				</ContentBox>
			</div>
		{/if}
	</div>
</SettingsCard>
