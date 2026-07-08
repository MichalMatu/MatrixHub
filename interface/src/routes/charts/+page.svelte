<script lang="ts">
	import { onMount, untrack } from 'svelte';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import DateSelector from './components/DateSelector.svelte';
	import EnvironmentHistoryChart from './components/charts/EnvironmentHistoryChart.svelte';
	import {
		parseBinaryLog,
		BinaryLogParseError,
		estimateRecordCount
	} from '$lib/utils/logs/binaryLogParser';
	import {
		LogsApiService,
		type LogListResponse
	} from '$lib/services/api/monitoring/LogsApiService';
	import { Logger } from '$lib/services/core/Logger';
	import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
	import { ApiError, getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
	import { Spinner } from '$lib/components';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	type Series = (number | null)[];
	type LogFileEntry = {
		date: string;
		path: string;
		size: number;
	};

	let chartData = $state<{
		timestamps: number[];
		co2: Series;
		temp: Series;
		humid: Series;
	}>({ timestamps: [], co2: [], temp: [], humid: [] });

	let availableDates = $state<string[]>([]);
	let availableLogFiles = $state<LogFileEntry[]>([]);
	let startDate = $state('');
	let endDate = $state('');
	let isLoading = $state(false);
	let logsListErrorMessage = $state<string | null>(null);
	let dataErrorMessage = $state<string | null>(null);
	let fallbackDate = $state<string | null>(null);
	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.charts_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.charts_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.charts_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/logs', label: m.menu_logs({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);

	// API Service
	const apiClient = useApiClient();
	let api = $derived(apiClient.createService(LogsApiService));

	function getTodayDate() {
		return new Date().toISOString().slice(0, 10);
	}

	function applySelectedRange(start: string, end = start) {
		startDate = start;
		endDate = end;
	}

	onMount(() => {
		fetchLogs();
	});

	function fetchLogs() {
		api
			.getLogsList()
			.then((data: LogListResponse) => {
				logsListErrorMessage = null;
				fallbackDate = null;
				availableDates = [];
				availableLogFiles = [];
				data.months?.forEach((month) => {
					month.files?.forEach((file) => {
						if (file.name.endsWith('.bin') && estimateRecordCount(file.size) > 0) {
							const date = file.name.replace('.bin', '');
							availableLogFiles.push({
								date,
								path: `${month.path}/${file.name}`,
								size: file.size
							});
						}
					});
				});

				availableLogFiles.sort((a, b) => a.date.localeCompare(b.date));
				availableDates = availableLogFiles.map((file) => file.date);

				if (availableDates.length > 0) {
					const latest = availableDates[availableDates.length - 1];
					applySelectedRange(latest);
				} else {
					applySelectedRange(getTodayDate());
				}
			})
			.catch((err: unknown) => {
				Logger.error('Failed to fetch logs list:', err);
				const kind = getRequestAbortKind(err);
				if (kind === 'abort') return;
				const prefix = toUserRequestErrorMessage(err, {
					timeoutMessage: m.charts_error_logs_list_timeout({ locale: i18n.languageTag }),
					fallbackMessage: m.charts_error_logs_list_fallback({ locale: i18n.languageTag })
				});
				const today = getTodayDate();
				logsListErrorMessage = `${prefix} ${m.charts_fallback_today({ locale: i18n.languageTag })}`;
				fallbackDate = today;
				applySelectedRange(today);
			});
	}

	function getFilesForRange(start: string, end: string) {
		if (!start || !end) return [];
		return availableLogFiles.filter((file) => file.date >= start && file.date <= end);
	}

	function mergeParsedLogs(files: ReturnType<typeof parseBinaryLog>[]) {
		const rows = files.flatMap((file) =>
			file.timestamps.map((timestamp, index) => ({
				timestamp,
				co2: file.co2s[index] ?? null,
				temp: file.temps[index] ?? null,
				humid: file.humids[index] ?? null
			}))
		);

		rows.sort((a, b) => a.timestamp - b.timestamp);

		return {
			timestamps: rows.map((row) => row.timestamp),
			co2: rows.map((row) => row.co2),
			temp: rows.map((row) => row.temp),
			humid: rows.map((row) => row.humid)
		};
	}

	async function fetchData(start?: string, end = start) {
		isLoading = true;
		dataErrorMessage = null;

		try {
			const rangeStart = start ?? getTodayDate();
			const rangeEnd = end ?? rangeStart;
			const files = getFilesForRange(rangeStart, rangeEnd);
			const parsedLogs: ReturnType<typeof parseBinaryLog>[] = [];

			for (const file of files) {
				try {
					const arrayBuffer = await api.getHistoricalChartDataByPath(file.path);
					parsedLogs.push(parseBinaryLog(arrayBuffer));
				} catch (err) {
					if (err instanceof ApiError && err.isNotFound) {
						continue;
					}
					if (err instanceof BinaryLogParseError) {
						Logger.warn('Skipping corrupt chart log:', file.path, err);
						continue;
					}
					throw err;
				}
			}

			chartData = mergeParsedLogs(parsedLogs);
		} catch (err) {
			const abortKind = getRequestAbortKind(err);
			if (abortKind === 'abort') {
				return;
			}
			if (err instanceof ApiError && err.isNotFound) {
				dataErrorMessage = null;
				chartData = { timestamps: [], co2: [], temp: [], humid: [] };
				return;
			}
			// When there are no logs at all (or corrupt zero-byte fallback logs), do not crash UI
			if (err instanceof BinaryLogParseError) {
				// Suppress the visible yellow error bar, let it naturally show "No Data Available"
				dataErrorMessage = null;
			} else {
				Logger.error('Failed to fetch/parse chart data:', err);
				dataErrorMessage = toUserRequestErrorMessage(err, {
					timeoutMessage: m.charts_error_data_timeout({ locale: i18n.languageTag }),
					fallbackMessage: m.charts_error_data_fallback({ locale: i18n.languageTag })
				});
			}
			chartData = { timestamps: [], co2: [], temp: [], humid: [] };
		} finally {
			isLoading = false;
		}
	}

	$effect(() => {
		// Prevent this effect from re-running due to unrelated reactive changes
		// (e.g. global stores/page data) while fetchData() is executing.
		// We only want to refetch when the selected range changes.
		const start = startDate;
		const end = endDate;
		if (start && end) {
			untrack(() => {
				fetchData(start, end);
			});
		}
	});
	import { PageWrapper } from '$lib/components/layout';
</script>

<PageWrapper>
	<div class="mb-3 flex justify-end">
		<HelpTriggerButton
			label={m.menu_help({ locale })}
			iconOnly={false}
			onclick={() => (helpOpen = true)}
		/>
	</div>

	{#if logsListErrorMessage}
		<div class="alert alert-warning mb-3">
			<span>{logsListErrorMessage}</span>
		</div>
	{/if}
	{#if dataErrorMessage}
		<div class="alert alert-warning mb-3">
			<span>{dataErrorMessage}</span>
		</div>
	{/if}
	{#if availableDates.length > 0}
		<DateSelector bind:startDate bind:endDate {availableDates} />
	{:else if fallbackDate}
		<div class="alert alert-info mb-3">
			<span>{m.charts_showing_day({ date: fallbackDate }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}

	<div class="relative min-h-[400px]">
		{#if isLoading}
			<div
				class="absolute inset-0 z-10 bg-base-100/60 backdrop-blur-[1px] flex justify-center pt-20 transition-all duration-300"
			>
				<Spinner text={m.charts_loading({ locale: i18n.languageTag })} />
			</div>
		{/if}

		{#if chartData.timestamps.length > 0}
			<div class="transition-opacity duration-300" class:opacity-30={isLoading}>
				<EnvironmentHistoryChart
					timestamps={chartData.timestamps}
					co2={chartData.co2}
					temperature={chartData.temp}
					humidity={chartData.humid}
				/>
			</div>
		{:else if !isLoading}
			<div class="alert alert-info">
				<span>{m.charts_no_data({ locale: i18n.languageTag })}</span>
			</div>
		{/if}
	</div>

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.charts_help_title({ locale })}
		intro={m.charts_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</PageWrapper>
