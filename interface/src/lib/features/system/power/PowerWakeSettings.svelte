<script lang="ts">
	import { FormToggle, FormInput } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { formatMs } from '$lib/features/system/power/formatPowerDuration';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import {
		BUTTON_WAKE_AWAKE_MAX_MINUTES,
		BUTTON_WAKE_AWAKE_MIN_MINUTES,
		TIMER_WAKE_AWAKE_MAX_MINUTES,
		TIMER_WAKE_AWAKE_MIN_MINUTES,
		TOUCH_GPIO_MAX,
		TOUCH_GPIO_MIN,
		TOUCH_THRESHOLD_MAX,
		TOUCH_THRESHOLD_MIN,
		WAKE_INTERVAL_MAX_MINUTES,
		WAKE_INTERVAL_MIN_MINUTES,
		clampMinutes,
		hasEnabledWakeSource,
		toMinutes,
		toMs,
		type PowerSettingsErrors
	} from './powerConfigModel';

	interface Props {
		errors: PowerSettingsErrors;
		localSleepEnabled: boolean;
		localWakeTimerEnabled: boolean;
		localWakeButtonEnabled: boolean;
		localWakeTouchEnabled: boolean;
		localWakeIntervalMs: number;
		localTimerWakeAwakeMs: number;
		localButtonWakeAwakeMs: number;
		localWakeTouchGpio: number;
		localWakeTouchThreshold: number;
		resetRevision: number;
		invalid?: boolean;
	}

	let {
		errors,
		localSleepEnabled,
		localWakeTimerEnabled = $bindable(),
		localWakeButtonEnabled = $bindable(),
		localWakeTouchEnabled = $bindable(),
		localWakeIntervalMs = $bindable(),
		localTimerWakeAwakeMs = $bindable(),
		localButtonWakeAwakeMs = $bindable(),
		localWakeTouchGpio = $bindable(),
		localWakeTouchThreshold = $bindable(),
		resetRevision,
		invalid = $bindable(false)
	}: Props = $props();

	let wakeIntervalMinutes = $state(0);
	let timerAwakeMinutes = $state(0);
	let buttonAwakeMinutes = $state(0);
	let editingWakeInterval = $state(false);
	let editingTimerAwake = $state(false);
	let editingButtonAwake = $state(false);

	$effect(() => {
		if (!Number.isFinite(resetRevision)) return;
		editingWakeInterval = false;
		editingTimerAwake = false;
		editingButtonAwake = false;
	});

	$effect(() => {
		if (!editingWakeInterval) {
			const next = toMinutes(localWakeIntervalMs);
			if (!Object.is(wakeIntervalMinutes, next)) wakeIntervalMinutes = next;
		}
	});

	$effect(() => {
		if (!editingTimerAwake) {
			const next = toMinutes(localTimerWakeAwakeMs);
			if (!Object.is(timerAwakeMinutes, next)) timerAwakeMinutes = next;
		}
	});

	$effect(() => {
		if (!editingButtonAwake) {
			const next = toMinutes(localButtonWakeAwakeMs);
			if (!Object.is(buttonAwakeMinutes, next)) buttonAwakeMinutes = next;
		}
	});

	const wakeIntervalInvalid = $derived(
		!Number.isFinite(wakeIntervalMinutes) ||
			wakeIntervalMinutes < WAKE_INTERVAL_MIN_MINUTES ||
			wakeIntervalMinutes > WAKE_INTERVAL_MAX_MINUTES
	);
	const timerAwakeInvalid = $derived(
		!Number.isFinite(timerAwakeMinutes) ||
			timerAwakeMinutes < TIMER_WAKE_AWAKE_MIN_MINUTES ||
			timerAwakeMinutes > TIMER_WAKE_AWAKE_MAX_MINUTES
	);
	const buttonAwakeInvalid = $derived(
		!Number.isFinite(buttonAwakeMinutes) ||
			buttonAwakeMinutes < BUTTON_WAKE_AWAKE_MIN_MINUTES ||
			buttonAwakeMinutes > BUTTON_WAKE_AWAKE_MAX_MINUTES
	);
	const touchGpioInvalid = $derived(
		!Number.isInteger(localWakeTouchGpio) ||
			localWakeTouchGpio < TOUCH_GPIO_MIN ||
			localWakeTouchGpio > TOUCH_GPIO_MAX
	);
	const touchThresholdInvalid = $derived(
		!Number.isInteger(localWakeTouchThreshold) ||
			localWakeTouchThreshold < TOUCH_THRESHOLD_MIN ||
			localWakeTouchThreshold > TOUCH_THRESHOLD_MAX
	);
	const wakeSourcesInvalid = $derived(
		localSleepEnabled &&
			!hasEnabledWakeSource({
				wake_timer_enabled: localWakeTimerEnabled,
				wake_button_enabled: localWakeButtonEnabled,
				wake_touch_enabled: localWakeTouchEnabled
			})
	);
	const hasValidationErrors = $derived(
		wakeIntervalInvalid ||
			timerAwakeInvalid ||
			buttonAwakeInvalid ||
			touchGpioInvalid ||
			touchThresholdInvalid ||
			wakeSourcesInvalid ||
			errors.wake_sources
	);

	$effect(() => {
		invalid = hasValidationErrors;
	});

	function applyWakeIntervalMinutes(next: number) {
		wakeIntervalMinutes = next;
		if (!Number.isFinite(next)) return;
		if (next < WAKE_INTERVAL_MIN_MINUTES || next > WAKE_INTERVAL_MAX_MINUTES) return;
		localWakeIntervalMs = toMs(next);
	}

	function applyTimerAwakeMinutes(next: number) {
		timerAwakeMinutes = next;
		if (!Number.isFinite(next)) return;
		if (next < TIMER_WAKE_AWAKE_MIN_MINUTES || next > TIMER_WAKE_AWAKE_MAX_MINUTES) return;
		localTimerWakeAwakeMs = toMs(next);
	}

	function applyButtonAwakeMinutes(next: number) {
		buttonAwakeMinutes = next;
		if (!Number.isFinite(next)) return;
		if (next < BUTTON_WAKE_AWAKE_MIN_MINUTES || next > BUTTON_WAKE_AWAKE_MAX_MINUTES) return;
		localButtonWakeAwakeMs = toMs(next);
	}

	function clampWakeIntervalOnBlur() {
		editingWakeInterval = false;
		const clamped = clampMinutes(
			wakeIntervalMinutes,
			WAKE_INTERVAL_MIN_MINUTES,
			WAKE_INTERVAL_MAX_MINUTES
		);
		wakeIntervalMinutes = clamped;
		localWakeIntervalMs = toMs(clamped);
	}

	function clampTimerAwakeOnBlur() {
		editingTimerAwake = false;
		const clamped = clampMinutes(
			timerAwakeMinutes,
			TIMER_WAKE_AWAKE_MIN_MINUTES,
			TIMER_WAKE_AWAKE_MAX_MINUTES
		);
		timerAwakeMinutes = clamped;
		localTimerWakeAwakeMs = toMs(clamped);
	}

	function clampButtonAwakeOnBlur() {
		editingButtonAwake = false;
		const clamped = clampMinutes(
			buttonAwakeMinutes,
			BUTTON_WAKE_AWAKE_MIN_MINUTES,
			BUTTON_WAKE_AWAKE_MAX_MINUTES
		);
		buttonAwakeMinutes = clamped;
		localButtonWakeAwakeMs = toMs(clamped);
	}
</script>

<FormToggle
	label={m.power_wake_timer_toggle_title({ locale: i18n.languageTag })}
	description={localWakeTimerEnabled
		? m.power_wake_timer_desc_on(
				{ time: formatMs(localWakeIntervalMs) },
				{ locale: i18n.languageTag }
			)
		: m.power_wake_timer_desc_off({ locale: i18n.languageTag })}
	bind:checked={localWakeTimerEnabled}
/>

<FormToggle
	label={m.power_wake_button_toggle_title({ locale: i18n.languageTag })}
	description={localWakeButtonEnabled
		? m.power_wake_button_desc_on(
				{ time: formatMs(localButtonWakeAwakeMs) },
				{ locale: i18n.languageTag }
			)
		: m.power_wake_button_desc_off({ locale: i18n.languageTag })}
	bind:checked={localWakeButtonEnabled}
/>

<FormToggle
	label={m.power_wake_touch_toggle_title({ locale: i18n.languageTag })}
	description={localWakeTouchEnabled
		? m.power_wake_touch_desc_on({ gpio: localWakeTouchGpio }, { locale: i18n.languageTag })
		: m.power_wake_touch_desc_off({ locale: i18n.languageTag })}
	bind:checked={localWakeTouchEnabled}
/>

{#if wakeSourcesInvalid || errors.wake_sources}
	<div class="alert alert-warning text-sm">
		{m.power_wake_sources_required({ locale: i18n.languageTag })}
	</div>
{/if}

<ContentBox>
	<FormInput
		label={m.power_wake_interval_title(
			{ unit: m.unit_min({ locale: i18n.languageTag }) },
			{ locale: i18n.languageTag }
		)}
		type="number"
		min={WAKE_INTERVAL_MIN_MINUTES}
		max={WAKE_INTERVAL_MAX_MINUTES}
		step={1}
		value={wakeIntervalMinutes}
		error={wakeIntervalInvalid || errors.wake_interval_ms
			? m.power_minutes_invalid_range(
					{
						min: WAKE_INTERVAL_MIN_MINUTES,
						max: WAKE_INTERVAL_MAX_MINUTES,
						unit: m.unit_min({ locale: i18n.languageTag })
					},
					{ locale: i18n.languageTag }
				)
			: undefined}
		onfocus={() => {
			editingWakeInterval = true;
		}}
		oninput={(e) => {
			const next = Number((e.target as HTMLInputElement).value);
			applyWakeIntervalMinutes(next);
		}}
		onblur={clampWakeIntervalOnBlur}
	>
		{#snippet suffix()}
			<span class="text-xs text-base-content/60">
				{m.unit_min({ locale: i18n.languageTag })}
			</span>
		{/snippet}
	</FormInput>
</ContentBox>

<ContentBox>
	<FormInput
		label={m.power_timer_awake_title(
			{ unit: m.unit_min({ locale: i18n.languageTag }) },
			{ locale: i18n.languageTag }
		)}
		type="number"
		min={TIMER_WAKE_AWAKE_MIN_MINUTES}
		max={TIMER_WAKE_AWAKE_MAX_MINUTES}
		step={1}
		value={timerAwakeMinutes}
		error={timerAwakeInvalid || errors.timer_wake_awake_ms
			? m.power_minutes_invalid_range(
					{
						min: TIMER_WAKE_AWAKE_MIN_MINUTES,
						max: TIMER_WAKE_AWAKE_MAX_MINUTES,
						unit: m.unit_min({ locale: i18n.languageTag })
					},
					{ locale: i18n.languageTag }
				)
			: undefined}
		onfocus={() => {
			editingTimerAwake = true;
		}}
		oninput={(e) => {
			const next = Number((e.target as HTMLInputElement).value);
			applyTimerAwakeMinutes(next);
		}}
		onblur={clampTimerAwakeOnBlur}
	>
		{#snippet suffix()}
			<span class="text-xs text-base-content/60">
				{m.unit_min({ locale: i18n.languageTag })}
			</span>
		{/snippet}
	</FormInput>
</ContentBox>

<ContentBox>
	<FormInput
		label={m.power_button_awake_title(
			{ unit: m.unit_min({ locale: i18n.languageTag }) },
			{ locale: i18n.languageTag }
		)}
		type="number"
		min={BUTTON_WAKE_AWAKE_MIN_MINUTES}
		max={BUTTON_WAKE_AWAKE_MAX_MINUTES}
		step={1}
		value={buttonAwakeMinutes}
		error={buttonAwakeInvalid || errors.button_wake_awake_ms
			? m.power_minutes_invalid_range(
					{
						min: BUTTON_WAKE_AWAKE_MIN_MINUTES,
						max: BUTTON_WAKE_AWAKE_MAX_MINUTES,
						unit: m.unit_min({ locale: i18n.languageTag })
					},
					{ locale: i18n.languageTag }
				)
			: undefined}
		onfocus={() => {
			editingButtonAwake = true;
		}}
		oninput={(e) => {
			const next = Number((e.target as HTMLInputElement).value);
			applyButtonAwakeMinutes(next);
		}}
		onblur={clampButtonAwakeOnBlur}
	>
		{#snippet suffix()}
			<span class="text-xs text-base-content/60">
				{m.unit_min({ locale: i18n.languageTag })}
			</span>
		{/snippet}
	</FormInput>
</ContentBox>

{#if localWakeTouchEnabled}
	<ContentBox>
		<div class="grid grid-cols-1 gap-3 sm:grid-cols-2">
			<FormInput
				label={m.power_touch_gpio_title({ locale: i18n.languageTag })}
				type="number"
				min={TOUCH_GPIO_MIN}
				max={TOUCH_GPIO_MAX}
				step={1}
				value={localWakeTouchGpio}
				error={touchGpioInvalid || errors.wake_touch_gpio
					? m.power_touch_gpio_invalid_range(
							{ min: TOUCH_GPIO_MIN, max: TOUCH_GPIO_MAX },
							{ locale: i18n.languageTag }
						)
					: undefined}
				oninput={(e) => {
					localWakeTouchGpio = Number((e.target as HTMLInputElement).value);
				}}
			/>

			<FormInput
				label={m.power_touch_threshold_title({ locale: i18n.languageTag })}
				type="number"
				min={TOUCH_THRESHOLD_MIN}
				max={TOUCH_THRESHOLD_MAX}
				step={100}
				value={localWakeTouchThreshold}
				error={touchThresholdInvalid || errors.wake_touch_threshold
					? m.power_touch_threshold_invalid_range(
							{ min: TOUCH_THRESHOLD_MIN, max: TOUCH_THRESHOLD_MAX },
							{ locale: i18n.languageTag }
						)
					: undefined}
				oninput={(e) => {
					localWakeTouchThreshold = Number((e.target as HTMLInputElement).value);
				}}
			/>
		</div>
	</ContentBox>
{/if}
