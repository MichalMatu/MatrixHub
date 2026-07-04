<script lang="ts">
	import { onMount } from 'svelte';
	import Temperature from '~icons/tabler/temperature';
	import Droplet from '~icons/tabler/droplet';
	import Cloud from '~icons/tabler/cloud';
	import { SensorCard } from '$lib/components/layout';
	import { calculateStats } from '$lib/utils/validation/sensorClassification';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { CHART_COLORS } from '$lib/constants';
	import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
	import {
		appendDashboardHistoryPoint,
		buildDashboardHistoryState,
		DASHBOARD_HISTORY_POINTS,
		EMPTY_DASHBOARD_HISTORY_STATE,
		type TelemetryHistorySnapshot
	} from '$lib/features/dashboard/dashboardTelemetry';
	import {
		DASHBOARD_SPARKLINE_SCALE_CONFIGS,
		resolveDashboardSparklineDomain
	} from '$lib/features/dashboard/dashboardSparklineScale';

	// Dashboard widgets
	import {
		BleWidget,
		ShellyWidget,
		WifiSensingWidget,
		AlarmsWidget,
		useDashboardWidgetVisibility
	} from '$lib/features/dashboard/widgets';

	type DashboardSensorData = {
		co2: number | null;
		temp: number | null;
		humid: number | null;
		lastReadOk?: boolean;
	};

	type TelemetrySnapshot = {
		co2: number;
		temp: number;
		humid: number;
		lastReadOk?: boolean;
		history?: TelemetryHistorySnapshot;
	};

	let sensorData = $state<DashboardSensorData>({
		co2: null,
		temp: null,
		humid: null
	});

	let history = $state(EMPTY_DASHBOARD_HISTORY_STATE);

	// Calculate statistics from history
	const stats = $derived({
		co2: calculateStats(history.co2.values),
		temp: calculateStats(history.temp.values),
		humid: calculateStats(history.humid.values)
	});

	const sparklineDomains = $derived({
		co2: resolveDashboardSparklineDomain(history.co2.values, DASHBOARD_SPARKLINE_SCALE_CONFIGS.co2),
		temp: resolveDashboardSparklineDomain(
			history.temp.values,
			DASHBOARD_SPARKLINE_SCALE_CONFIGS.temp
		),
		humid: resolveDashboardSparklineDomain(
			history.humid.values,
			DASHBOARD_SPARKLINE_SCALE_CONFIGS.humid
		)
	});

	function applyTelemetrySnapshot(data: TelemetrySnapshot) {
		sensorData = {
			...sensorData,
			co2: data.co2,
			temp: data.temp,
			humid: data.humid,
			lastReadOk: data.lastReadOk
		};
	}

	function applyHistorySnapshot(historyData?: TelemetryHistorySnapshot) {
		if (!historyData) return;

		history = buildDashboardHistoryState(historyData, DASHBOARD_HISTORY_POINTS);
	}

	const telemetryChannel = createSystemChannelSubscription<TelemetrySnapshot>({
		channel: 'telemetry',
		onSnapshot: (data) => {
			applyTelemetrySnapshot(data);
			applyHistorySnapshot(data.history);
		},
		onEvent: (event) => {
			if (event.type !== 'sensor') return;

			sensorData = {
				...sensorData,
				co2: event.data.co2,
				temp: event.data.temp,
				humid: event.data.humid,
				lastReadOk: event.data.lastReadOk
			};

			// Append new data point to chart history for real-time sparkline updates
			if (history.co2.timestamps.length > 0) {
				// WS sensor timestamp is uptime (millis), not epoch seconds.
				const nowSec = Math.floor(Date.now() / 1000);
				const lastTs = history.co2.timestamps[history.co2.timestamps.length - 1] ?? nowSec;
				const ts = Math.max(nowSec, lastTs + 1);
				history = appendDashboardHistoryPoint(
					history,
					{
						co2: event.data.co2,
						temp: event.data.temp,
						humid: event.data.humid
					},
					ts,
					DASHBOARD_HISTORY_POINTS
				);
			} else {
				const nowSec = Math.floor(Date.now() / 1000);
				history = appendDashboardHistoryPoint(
					EMPTY_DASHBOARD_HISTORY_STATE,
					{
						co2: event.data.co2,
						temp: event.data.temp,
						humid: event.data.humid
					},
					nowSec,
					DASHBOARD_HISTORY_POINTS
				);
			}
		}
	});

	const widgetVisibility = useDashboardWidgetVisibility();

	onMount(() => {
		telemetryChannel.subscribe();

		return () => {
			telemetryChannel.destroy();
		};
	});

	import { PageWrapper } from '$lib/components/layout';
</script>

<PageWrapper>
	<div class="grid grid-cols-1 gap-3 max-[320px]:gap-1 sm:gap-4 sm:grid-cols-2 lg:grid-cols-3">
		<!-- CO2 Card -->
		<SensorCard
			shadowColor="success"
			sparklineId="dashboard-co2"
			title="CO₂"
			icon={Cloud}
			value={sensorData.co2}
			unit="ppm"
			hasError={sensorData.lastReadOk === false}
			history={history.co2.values}
			timestamps={history.co2.timestamps}
			stats={stats.co2}
			chartColor={CHART_COLORS.co2}
			domainMin={sparklineDomains.co2?.min}
			domainMax={sparklineDomains.co2?.max}
		/>

		<!-- Temperature Card -->
		<SensorCard
			shadowColor="error"
			sparklineId="dashboard-temp"
			title={m.dashboard_temp({ locale: i18n.languageTag })}
			icon={Temperature}
			value={sensorData.temp}
			unit="°C"
			history={history.temp.values}
			timestamps={history.temp.timestamps}
			stats={stats.temp}
			chartColor={CHART_COLORS.temperature}
			domainMin={sparklineDomains.temp?.min}
			domainMax={sparklineDomains.temp?.max}
		/>

		<!-- Humidity Card -->
		<SensorCard
			shadowColor="info"
			sparklineId="dashboard-humid"
			title={m.dashboard_humid({ locale: i18n.languageTag })}
			icon={Droplet}
			value={sensorData.humid}
			unit="%"
			history={history.humid.values}
			timestamps={history.humid.timestamps}
			stats={stats.humid}
			chartColor={CHART_COLORS.humidity}
			domainMin={sparklineDomains.humid?.min}
			domainMax={sparklineDomains.humid?.max}
		/>

		<!-- Devices & Integrations -->
		{#if widgetVisibility.hasVisibilityDecision}
			{#if widgetVisibility.showBle}
				<BleWidget />
			{/if}

			{#if widgetVisibility.showShelly}
				<ShellyWidget />
			{/if}

			{#if widgetVisibility.showAlarms}
				<AlarmsWidget />
			{/if}

			{#if widgetVisibility.showWifiSensing}
				<WifiSensingWidget />
			{/if}
		{:else}
			{#each Array(2) as _, index (index)}
				<div
					class="card bg-base-200/70 min-h-36 animate-pulse border border-base-300/50"
					aria-hidden="true"
				>
					<div class="card-body gap-3">
						<div class="h-4 w-28 rounded bg-base-content/10"></div>
						<div class="h-16 rounded bg-base-content/8"></div>
						<div class="h-3 w-2/3 rounded bg-base-content/8"></div>
					</div>
				</div>
			{/each}
		{/if}
	</div>
</PageWrapper>
