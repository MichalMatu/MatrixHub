<script lang="ts">
	let {
		startDate = $bindable(''),
		endDate = $bindable(''),
		availableDates = [],
		onHelp = undefined
	}: {
		startDate: string;
		endDate: string;
		availableDates: string[];
		onHelp?: () => void;
	} = $props();

	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import FormInput from '$lib/components/shared/forms/FormInput.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let sortedDates = $derived([...availableDates].sort());
	let minDate = $derived(sortedDates[0] ?? '');
	let maxDate = $derived(sortedDates[sortedDates.length - 1] ?? '');
	let selectedDates = $derived(
		startDate && endDate ? sortedDates.filter((date) => date >= startDate && date <= endDate) : []
	);

	function firstAvailableOnOrAfter(date: string) {
		return sortedDates.find((availableDate) => availableDate >= date) ?? maxDate;
	}

	function lastAvailableOnOrBefore(date: string) {
		for (let index = sortedDates.length - 1; index >= 0; index--) {
			const availableDate = sortedDates[index];
			if (availableDate <= date) return availableDate;
		}
		return minDate;
	}

	function normalizeStart(date: string) {
		if (sortedDates.length === 0) return '';
		const bounded = date < minDate ? minDate : date > maxDate ? maxDate : date;
		return sortedDates.includes(bounded) ? bounded : firstAvailableOnOrAfter(bounded);
	}

	function normalizeEnd(date: string) {
		if (sortedDates.length === 0) return '';
		const bounded = date < minDate ? minDate : date > maxDate ? maxDate : date;
		return sortedDates.includes(bounded) ? bounded : lastAvailableOnOrBefore(bounded);
	}

	function handleStartInput(event: Event) {
		const value = (event.currentTarget as HTMLInputElement).value;
		const nextStart = normalizeStart(value);
		startDate = nextStart;
		if (!endDate || endDate < nextStart) {
			endDate = nextStart;
		}
	}

	function handleEndInput(event: Event) {
		const value = (event.currentTarget as HTMLInputElement).value;
		const nextEnd = normalizeEnd(value);
		endDate = nextEnd;
		if (!startDate || startDate > nextEnd) {
			startDate = nextEnd;
		}
	}

	$effect(() => {
		if (sortedDates.length === 0) {
			startDate = '';
			endDate = '';
			return;
		}

		const repairedStart = startDate ? normalizeStart(startDate) : maxDate;
		const repairedEnd = endDate ? normalizeEnd(endDate) : repairedStart;

		startDate = repairedStart;
		endDate = repairedEnd < repairedStart ? repairedStart : repairedEnd;
	});
</script>

<div class="mb-4 rounded-md border border-base-content/10 bg-base-100/35 p-3">
	<div class="grid gap-3 sm:grid-cols-[minmax(0,1fr)_minmax(0,1fr)_auto] sm:items-end">
		<div class="min-w-0">
			<FormInput
				label={m.charts_range_from({ locale: i18n.languageTag })}
				type="date"
				value={startDate}
				min={minDate}
				max={maxDate}
				disabled={sortedDates.length === 0}
				onchange={handleStartInput}
			/>
		</div>

		<div class="min-w-0">
			<FormInput
				label={m.charts_range_to({ locale: i18n.languageTag })}
				type="date"
				value={endDate}
				min={minDate}
				max={maxDate}
				disabled={sortedDates.length === 0}
				onchange={handleEndInput}
			/>
		</div>

		<div
			class="flex items-center justify-between gap-2 pb-1 text-xs text-base-content/55 sm:min-w-40 sm:justify-end"
		>
			<span class="min-w-0 text-right">
				{#if selectedDates.length > 0}
					{m.charts_range_days_with_data(
						{ count: selectedDates.length },
						{ locale: i18n.languageTag }
					)}
				{:else}
					{m.charts_range_no_days({ locale: i18n.languageTag })}
				{/if}
			</span>
			{#if onHelp}
				<HelpTriggerButton label={m.menu_help({ locale: i18n.languageTag })} onclick={onHelp} />
			{/if}
		</div>
	</div>
</div>
