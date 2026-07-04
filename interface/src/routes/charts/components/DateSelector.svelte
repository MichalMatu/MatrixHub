<script lang="ts">
	let {
		selectedDate = $bindable(''),
		currentMonth = $bindable(''),
		availableDates = [],
		showCalendar = $bindable(false)
	}: {
		selectedDate: string;
		currentMonth: string;
		availableDates: string[];
		showCalendar: boolean;
	} = $props();

	// Extract unique months from available dates
	let availableMonths = $derived(
		Array.from(new Set(availableDates.map((date) => date.slice(0, 7)))).sort()
	);

	// Check if we can navigate to prev/next month
	let currentMonthIndex = $derived(availableMonths.indexOf(currentMonth));
	let canGoPrevMonth = $derived(currentMonthIndex > 0);
	let canGoNextMonth = $derived(
		currentMonthIndex < availableMonths.length - 1 && currentMonthIndex >= 0
	);

	// Check if we can navigate to prev/next day
	let sortedDates = $derived([...availableDates].sort());
	let currentDateIndex = $derived(selectedDate ? sortedDates.indexOf(selectedDate) : -1);
	let canGoPrevDay = $derived(currentDateIndex > 0);
	let canGoNextDay = $derived(currentDateIndex >= 0 && currentDateIndex < sortedDates.length - 1);

	function getDatesForMonth(yearMonth: string) {
		return sortedDates.filter((date) => date.startsWith(`${yearMonth}-`));
	}

	// Auto-select newest month with data when available dates change
	$effect(() => {
		if (availableMonths.length > 0 && !availableMonths.includes(currentMonth)) {
			currentMonth = availableMonths[availableMonths.length - 1];
		}
	});

	$effect(() => {
		if (sortedDates.length === 0) return;
		if (selectedDate && sortedDates.includes(selectedDate)) return;

		const datesForMonth = getDatesForMonth(currentMonth);
		selectedDate = datesForMonth[datesForMonth.length - 1] ?? sortedDates[sortedDates.length - 1];
		currentMonth = selectedDate.slice(0, 7);
	});

	function changeMonth(delta: number) {
		if (delta < 0 && !canGoPrevMonth) return;
		if (delta > 0 && !canGoNextMonth) return;

		const newIndex = currentMonthIndex + delta;
		if (newIndex >= 0 && newIndex < availableMonths.length) {
			const nextMonth = availableMonths[newIndex];
			const datesForMonth = getDatesForMonth(nextMonth);
			currentMonth = nextMonth;
			if (datesForMonth.length > 0) {
				selectedDate = datesForMonth[datesForMonth.length - 1];
			}
		}
	}

	function changeDay(delta: number) {
		if (delta < 0 && !canGoPrevDay) return;
		if (delta > 0 && !canGoNextDay) return;

		const newIndex = currentDateIndex + delta;
		if (newIndex >= 0 && newIndex < sortedDates.length) {
			selectedDate = sortedDates[newIndex];
			// Update month to match the new date
			currentMonth = selectedDate.slice(0, 7);
		}
	}

	function getDaysInMonth(yearMonth: string) {
		const [year, month] = yearMonth.split('-').map(Number);
		return new Date(year, month, 0).getDate();
	}

	function handleDateSelect(dateStr: string) {
		selectedDate = dateStr;
		showCalendar = false;
	}

	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
</script>

<div class="mb-4">
	<div class="flex items-center gap-2">
		<!-- Month navigation -->
		<!-- Month navigation -->
		<FormButton
			label="◀◀"
			class="btn-ghost btn-sm"
			disabled={!canGoPrevMonth}
			onclick={() => changeMonth(-1)}
			title={m.charts_nav_prev_month({ locale: i18n.languageTag })}
		/>

		<!-- Day navigation -->
		<FormButton
			label="◀"
			class="btn-ghost btn-sm"
			disabled={!canGoPrevDay}
			onclick={() => changeDay(-1)}
			title={m.charts_nav_prev_day({ locale: i18n.languageTag })}
		/>

		<!-- Date display / calendar toggle -->
		<button
			type="button"
			class="btn btn-outline btn-sm flex-1"
			onclick={() => (showCalendar = !showCalendar)}
		>
			{selectedDate || currentMonth}
			{#if availableMonths.length > 0}
				<span class="text-xs opacity-60 ml-1">
					({currentMonthIndex + 1}/{availableMonths.length})
				</span>
			{/if}
		</button>

		<!-- Day navigation -->
		<FormButton
			label="▶"
			class="btn-ghost btn-sm"
			disabled={!canGoNextDay}
			onclick={() => changeDay(1)}
			title={m.charts_nav_next_day({ locale: i18n.languageTag })}
		/>

		<!-- Month navigation -->
		<FormButton
			label="▶▶"
			class="btn-ghost btn-sm"
			disabled={!canGoNextMonth}
			onclick={() => changeMonth(1)}
			title={m.charts_nav_next_month({ locale: i18n.languageTag })}
		/>
	</div>
	{#if showCalendar}
		<BaseCard class="mt-2">
			<div class="grid grid-cols-7 gap-1">
				{#each Array(getDaysInMonth(currentMonth)) as _, i (i)}
					{@const day = i + 1}
					{@const dateStr = `${currentMonth}-${String(day).padStart(2, '0')}`}
					{@const hasData = availableDates.includes(dateStr)}
					<FormButton
						label={String(day)}
						class="btn-xs {selectedDate === dateStr
							? 'btn-primary'
							: hasData
								? 'btn-outline'
								: 'btn-ghost'} {hasData ? 'font-bold' : ''}"
						disabled={!hasData}
						onclick={() => handleDateSelect(dateStr)}
					/>
				{/each}
			</div>
		</BaseCard>
	{/if}
</div>
