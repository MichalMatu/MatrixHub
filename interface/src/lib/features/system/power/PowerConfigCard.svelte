<script lang="ts">
	import { Spinner } from '$lib/components';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormToggle, FormInput } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';
	import PowerWakeSettings from './PowerWakeSettings.svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import PowerIcon from '~icons/tabler/power';
	import Timer from '~icons/tabler/hourglass';
	import Interval from '~icons/tabler/repeat';
	import Cpu from '~icons/tabler/cpu';
	import Thermometer from '~icons/tabler/temperature';
	import type { PowerStatus } from '$lib/types/system/power';
	import { formatMs } from '$lib/features/system/power/formatPowerDuration';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { createFeatureLinks, powerHelpLinkIds } from '$lib/features/navigation/featureRegistry';
	import {
		GRACE_MAX_MINUTES,
		GRACE_MIN_MINUTES,
		INACTIVITY_MAX_MINUTES,
		INACTIVITY_MIN_MINUTES,
		clampMinutes,
		toMinutes,
		toMs,
		type PowerSettingsErrors
	} from './powerConfigModel';

	interface Props {
		status: PowerStatus | null;
		loading: boolean;
		error: string | null;
		errors: PowerSettingsErrors;
		localSleepEnabled: boolean;
		localInactivityTimeoutMs: number;
		localGraceAfterBootMs: number;
		localWakeTimerEnabled: boolean;
		localWakeButtonEnabled: boolean;
		localWakeTouchEnabled: boolean;
		localWakeIntervalMs: number;
		localTimerWakeAwakeMs: number;
		localButtonWakeAwakeMs: number;
		localWakeTouchGpio: number;
		localWakeTouchThreshold: number;
		hasChanges: boolean;
		saving: boolean;
		onSave: () => void;
		onReset: () => void;
	}

	let {
		status,
		loading,
		error,
		errors,
		localSleepEnabled = $bindable(),
		localInactivityTimeoutMs = $bindable(),
		localGraceAfterBootMs = $bindable(),
		localWakeTimerEnabled = $bindable(),
		localWakeButtonEnabled = $bindable(),
		localWakeTouchEnabled = $bindable(),
		localWakeIntervalMs = $bindable(),
		localTimerWakeAwakeMs = $bindable(),
		localButtonWakeAwakeMs = $bindable(),
		localWakeTouchGpio = $bindable(),
		localWakeTouchThreshold = $bindable(),
		hasChanges,
		saving,
		onSave,
		onReset
	}: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';

	let inactivityMinutes = $state(0);
	let graceMinutes = $state(0);
	let editingInactivity = $state(false);
	let editingGrace = $state(false);
	let wakeSettingsInvalid = $state(false);
	let wakeResetRevision = $state(0);

	$effect(() => {
		if (!editingInactivity) {
			const next = toMinutes(localInactivityTimeoutMs);
			if (inactivityMinutes !== next) inactivityMinutes = next;
		}
	});

	$effect(() => {
		if (!editingGrace) {
			const next = toMinutes(localGraceAfterBootMs);
			if (graceMinutes !== next) graceMinutes = next;
		}
	});

	const inactivityInvalid = $derived(
		!Number.isFinite(inactivityMinutes) ||
			inactivityMinutes < INACTIVITY_MIN_MINUTES ||
			inactivityMinutes > INACTIVITY_MAX_MINUTES
	);
	const graceInvalid = $derived(
		!Number.isFinite(graceMinutes) ||
			graceMinutes < GRACE_MIN_MINUTES ||
			graceMinutes > GRACE_MAX_MINUTES
	);
	const hasValidationErrors = $derived(inactivityInvalid || graceInvalid || wakeSettingsInvalid);

	function applyInactivityMinutes(next: number) {
		inactivityMinutes = next;
		if (!Number.isFinite(next)) return;
		if (next < INACTIVITY_MIN_MINUTES || next > INACTIVITY_MAX_MINUTES) return;
		localInactivityTimeoutMs = toMs(next);
	}

	function applyGraceMinutes(next: number) {
		graceMinutes = next;
		if (!Number.isFinite(next)) return;
		if (next < GRACE_MIN_MINUTES || next > GRACE_MAX_MINUTES) return;
		localGraceAfterBootMs = toMs(next);
	}

	function clampInactivityOnBlur() {
		editingInactivity = false;
		const clamped = clampMinutes(inactivityMinutes, INACTIVITY_MIN_MINUTES, INACTIVITY_MAX_MINUTES);
		inactivityMinutes = clamped;
		localInactivityTimeoutMs = toMs(clamped);
	}

	function clampGraceOnBlur() {
		editingGrace = false;
		const clamped = clampMinutes(graceMinutes, GRACE_MIN_MINUTES, GRACE_MAX_MINUTES);
		graceMinutes = clamped;
		localGraceAfterBootMs = toMs(clamped);
	}

	function handleSubmit(event: Event) {
		event.preventDefault();
		handleSave();
	}

	function handleSave() {
		clampInactivityOnBlur();
		clampGraceOnBlur();
		onSave();
	}

	function handleReset() {
		editingInactivity = false;
		editingGrace = false;
		wakeResetRevision += 1;
		onReset();
	}

	function formatTemperature(value: number | null | undefined): string {
		return typeof value === 'number' && Number.isFinite(value) ? `${value.toFixed(1)} °C` : '--';
	}

	function thermalStateLabel(state: PowerStatus['thermal_state']): string {
		switch (state) {
			case 'soft_throttle':
				return m.power_thermal_state_soft({ locale: i18n.languageTag });
			case 'hard_throttle':
				return m.power_thermal_state_hard({ locale: i18n.languageTag });
			case 'critical':
				return m.power_thermal_state_critical({ locale: i18n.languageTag });
			case 'normal':
				return m.power_thermal_state_normal({ locale: i18n.languageTag });
			default:
				return m.power_thermal_state_unknown({ locale: i18n.languageTag });
		}
	}

	function thermalValueClass(state: PowerStatus['thermal_state']): string {
		switch (state) {
			case 'critical':
				return 'text-sm text-error font-semibold';
			case 'hard_throttle':
				return 'text-sm text-error';
			case 'soft_throttle':
				return 'text-sm text-warning';
			default:
				return 'text-sm text-base-content';
		}
	}

	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.power_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.power_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.power_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived(createFeatureLinks(powerHelpLinkIds, locale));
</script>

<SettingsCard
	title={m.power_config_title({ locale: i18n.languageTag })}
	icon={PowerIcon}
	{hasChanges}
	{loading}
	{saving}
	disabled={hasValidationErrors}
	onSave={!loading && !error && status ? handleSave : undefined}
	onReset={handleReset}
	dirtySourceId="power-settings"
>
	{#snippet actions()}
		<HelpTriggerButton label={m.power_help_title({ locale })} onclick={() => (helpOpen = true)} />
	{/snippet}

	{#if loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else if error}
		<div class="alert alert-error text-sm">
			{m.power_config_unavailable({ locale: i18n.languageTag })}
		</div>
	{:else if status}
		<form
			onsubmit={handleSubmit}
			class="flex w-full flex-col gap-1"
			transition:slide|local={{ duration: 300, easing: cubicOut }}
			novalidate
		>
			<FormToggle
				label={m.power_autosleep_title({ locale: i18n.languageTag })}
				description={localSleepEnabled
					? m.power_autosleep_desc_on(
							{ time: formatMs(localInactivityTimeoutMs) },
							{ locale: i18n.languageTag }
						)
					: m.power_autosleep_desc_off({ locale: i18n.languageTag })}
				bind:checked={localSleepEnabled}
			/>

			<ContentBox>
				<FormInput
					label={m.power_inactivity_title(
						{ unit: m.unit_min({ locale: i18n.languageTag }) },
						{ locale: i18n.languageTag }
					)}
					type="number"
					min={INACTIVITY_MIN_MINUTES}
					max={INACTIVITY_MAX_MINUTES}
					step={1}
					value={inactivityMinutes}
					error={inactivityInvalid || errors.inactivity_timeout_ms
						? m.power_inactivity_invalid_range(
								{
									min: INACTIVITY_MIN_MINUTES,
									max: INACTIVITY_MAX_MINUTES,
									unit: m.unit_min({ locale: i18n.languageTag })
								},
								{ locale: i18n.languageTag }
							)
						: undefined}
					onfocus={() => {
						editingInactivity = true;
					}}
					oninput={(e) => {
						const next = Number((e.target as HTMLInputElement).value);
						applyInactivityMinutes(next);
					}}
					onblur={clampInactivityOnBlur}
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
					label={m.power_grace_title(
						{ unit: m.unit_min({ locale: i18n.languageTag }) },
						{ locale: i18n.languageTag }
					)}
					type="number"
					min={GRACE_MIN_MINUTES}
					max={GRACE_MAX_MINUTES}
					step={1}
					value={graceMinutes}
					error={graceInvalid || errors.grace_after_boot_ms
						? m.power_grace_invalid_range(
								{
									min: GRACE_MIN_MINUTES,
									max: GRACE_MAX_MINUTES,
									unit: m.unit_min({ locale: i18n.languageTag })
								},
								{ locale: i18n.languageTag }
							)
						: undefined}
					onfocus={() => {
						editingGrace = true;
					}}
					oninput={(e) => {
						const next = Number((e.target as HTMLInputElement).value);
						applyGraceMinutes(next);
					}}
					onblur={clampGraceOnBlur}
				>
					{#snippet suffix()}
						<span class="text-xs text-base-content/60">
							{m.unit_min({ locale: i18n.languageTag })}
						</span>
					{/snippet}
				</FormInput>
			</ContentBox>

			<PowerWakeSettings
				{errors}
				{localSleepEnabled}
				resetRevision={wakeResetRevision}
				bind:invalid={wakeSettingsInvalid}
				bind:localWakeTimerEnabled
				bind:localWakeButtonEnabled
				bind:localWakeTouchEnabled
				bind:localWakeIntervalMs
				bind:localTimerWakeAwakeMs
				bind:localButtonWakeAwakeMs
				bind:localWakeTouchGpio
				bind:localWakeTouchThreshold
			/>

			<StatusRow
				icon={Interval}
				iconClass={statIconClass}
				label={m.power_wake_title({ locale: i18n.languageTag })}
				value={formatMs(status.wake_interval_ms)}
			/>

			<StatusRow
				icon={Thermometer}
				iconClass={statIconClass}
				label={m.power_thermal_title({ locale: i18n.languageTag })}
				value={`${formatTemperature(status.thermal_temp_c)} · ${thermalStateLabel(status.thermal_state)}`}
				valueClass={thermalValueClass(status.thermal_state)}
			/>

			<StatusRow
				icon={Cpu}
				iconClass={statIconClass}
				label={m.power_thermal_cpu_title({ locale: i18n.languageTag })}
				value={`${status.thermal_cpu_mhz} MHz`}
				valueClass={status.thermal_throttled ? 'text-sm text-warning' : 'text-sm text-base-content'}
			/>

			{#if status.sleep_requested}
				<StatusRow
					icon={Timer}
					iconClass={statIconClass}
					label={m.power_sleep_eta({ locale: i18n.languageTag })}
					value={formatMs(status.sleep_eta_ms)}
					valueClass="text-sm text-warning font-semibold"
				/>
			{/if}

			{#if status.wake_awake_eta_ms > 0}
				<StatusRow
					icon={Timer}
					iconClass={statIconClass}
					label={m.power_wake_awake_eta({ locale: i18n.languageTag })}
					value={formatMs(status.wake_awake_eta_ms)}
					valueClass="text-sm text-warning font-semibold"
				/>
			{/if}
		</form>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.power_help_title({ locale })}
		intro={m.power_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
