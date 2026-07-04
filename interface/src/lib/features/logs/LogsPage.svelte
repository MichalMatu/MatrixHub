<script lang="ts">
	import { useLogStatistics } from './useLogStatistics.svelte';
	import { useLogsManagement } from './useLogsManagement.svelte';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import LogEmptyState from './LogEmptyState.svelte';
	import LogStatisticsCard from './LogStatisticsCard.svelte';
	import LogPeriodCard from './LogPeriodCard.svelte';
	import LogMonthCard from './LogMonthCard.svelte';
	import { PageWrapper, GridLayout } from '$lib/components/layout';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import Calendar from '~icons/tabler/calendar';
	import ChartBar from '~icons/tabler/chart-bar';
	import Clock from '~icons/tabler/clock';

	const logsState = useLogsManagement();
	const stats = useLogStatistics(() => logsState.logs);
	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.logs_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.logs_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.logs_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/charts', label: m.menu_charts({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<PageWrapper>
	<div class="mb-3 flex justify-end">
		<HelpTriggerButton
			label={m.menu_help({ locale })}
			iconOnly={false}
			onclick={() => (helpOpen = true)}
		/>
	</div>

	{#if logsState.loading}
		<GridLayout cols={2}>
			<div class="flex flex-col gap-3 md:gap-4">
				<LoadingCard
					title={m.logs_title({ locale: i18n.languageTag })}
					icon={Calendar}
					loading={true}
					minHeight="192px"
				/>
				<LoadingCard
					title={m.logs_title({ locale: i18n.languageTag })}
					icon={Calendar}
					loading={true}
					minHeight="192px"
				/>
			</div>
			<div class="flex flex-col gap-3 md:gap-4">
				<LoadingCard
					title={m.logs_stats_title({ locale: i18n.languageTag })}
					icon={ChartBar}
					loading={true}
					minHeight="256px"
				/>
				<LoadingCard
					title={m.logs_period_title({ locale: i18n.languageTag })}
					icon={Clock}
					loading={true}
					minHeight="128px"
				/>
			</div>
		</GridLayout>
	{:else if logsState.error}
		<div class="alert alert-warning mb-3">
			<span>{logsState.error}</span>
		</div>
	{:else if logsState.logs.months.length === 0}
		<LogEmptyState />
	{:else}
		<GridLayout cols={2}>
			<!-- Left column: Log files -->
			<div class="flex flex-col gap-3 md:gap-4">
				{#each logsState.logs.months as month}
					<LogMonthCard
						{month}
						canDelete={logsState.canManage}
						onDownload={logsState.downloadLog}
						onDelete={logsState.confirmDelete}
					/>
				{/each}
			</div>

			<!-- Right column: Statistics + Period -->
			<div class="flex flex-col gap-3 md:gap-4">
				<LogStatisticsCard
					totalFiles={stats.totalFiles}
					totalSizeKB={stats.totalSizeKB}
					averageSizeKB={stats.averageSizeKB}
					estimatedEntries={stats.estimatedEntries}
				/>

				<LogPeriodCard
					oldestMonth={stats.oldestMonth}
					newestMonth={stats.newestMonth}
					totalMonths={logsState.logs.months.length}
				/>
			</div>
		</GridLayout>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.logs_help_title({ locale })}
		intro={m.logs_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</PageWrapper>
