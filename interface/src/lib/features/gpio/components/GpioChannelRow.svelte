<script lang="ts">
	import Circle from '~icons/tabler/circle-filled';
	import Power from '~icons/tabler/power';
	import type {
		GpioChannelConfig,
		GpioChannelStatus,
		GpioMode,
		GpioPinDefinition,
		GpioPull
	} from '$lib/types/domain/gpio';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormButton, FormInput, FormSelect, FormToggle } from '$lib/components/shared/forms';
	import {
		GPIO_DEBOUNCE_MAX_MS,
		GPIO_DEBOUNCE_MIN_MS,
		createGpioModeOptions,
		createGpioPullOptions,
		getGpioModeLabel
	} from '../gpioModel';

	let {
		channel,
		status = undefined,
		pin = undefined,
		canManage = false,
		hasChanges = false,
		saving = false,
		outputBusy = false,
		onChange,
		onOutputChange
	}: {
		channel: GpioChannelConfig;
		status?: GpioChannelStatus;
		pin?: GpioPinDefinition;
		canManage?: boolean;
		hasChanges?: boolean;
		saving?: boolean;
		outputBusy?: boolean;
		onChange: (id: string, patch: Partial<GpioChannelConfig>) => void;
		onOutputChange: (id: string, value: boolean) => void;
	} = $props();

	const modeOptions = $derived(createGpioModeOptions());
	const pullOptions = $derived(
		createGpioPullOptions({
			allowPullup: pin?.pull_up,
			allowPulldown: pin?.pull_down
		})
	);
	const controlsDisabled = $derived(!canManage || saving);
	const outputDisabled = $derived(
		controlsDisabled || hasChanges || outputBusy || status?.mode !== 'output' || !status.configured
	);
	const logicalValue = $derived(status?.logical ?? channel.initial_output);
	const stateBadgeClass = $derived(
		status?.configured ? (status.logical ? 'badge-success' : 'badge-neutral') : 'badge-outline'
	);

	function handleModeChange(event: Event) {
		const mode = (event.target as HTMLSelectElement).value as GpioMode;
		onChange(channel.id, { mode });
	}

	function handleDisable() {
		onChange(channel.id, { mode: 'disabled' });
	}

	function handlePullChange(event: Event) {
		const pull = (event.target as HTMLSelectElement).value as GpioPull;
		onChange(channel.id, { pull });
	}

	function handleDebounceInput(event: Event) {
		const debounceMs = Number((event.target as HTMLInputElement).value);
		onChange(channel.id, { debounce_ms: debounceMs });
	}

	function handleInvertChange(event: Event) {
		onChange(channel.id, { inverted: (event.target as HTMLInputElement).checked });
	}

	function handleOutputChange(event: Event) {
		onOutputChange(channel.id, (event.target as HTMLInputElement).checked);
	}
</script>

<ContentBox
	paddingClass="p-3"
	class="grid gap-3 xl:grid-cols-[minmax(12rem,0.85fr)_minmax(17rem,1fr)_minmax(20rem,1.35fr)_auto] xl:items-center"
>
	<div class="flex-1 min-w-0">
		<div class="flex flex-wrap items-center gap-2">
			<div class="font-bold">{channel.name}</div>
			<span class="badge badge-sm {stateBadgeClass}">
				{logicalValue
					? m.gpio_true({ locale: i18n.languageTag })
					: m.gpio_false({ locale: i18n.languageTag })}
			</span>
		</div>
		<div class="mt-1 flex flex-wrap items-center gap-2 text-xs opacity-70">
			<span>{m.gpio_pin({ pin: channel.pin }, { locale: i18n.languageTag })}</span>
			<span>·</span>
			<span>{getGpioModeLabel(channel.mode)}</span>
		</div>
	</div>

	<div class="grid grid-cols-3 gap-2 text-xs">
		<div class="min-w-0 rounded-md border border-base-300/50 bg-base-100/20 px-2 py-1.5">
			<div class="truncate opacity-60">{m.gpio_logical({ locale: i18n.languageTag })}</div>
			<div class="truncate font-bold">
				{logicalValue
					? m.gpio_true({ locale: i18n.languageTag })
					: m.gpio_false({ locale: i18n.languageTag })}
			</div>
		</div>
		<div class="min-w-0 rounded-md border border-base-300/50 bg-base-100/20 px-2 py-1.5">
			<div class="truncate opacity-60">{m.gpio_raw({ locale: i18n.languageTag })}</div>
			<div class="truncate font-bold">
				{status?.raw
					? m.gpio_high({ locale: i18n.languageTag })
					: m.gpio_low({ locale: i18n.languageTag })}
			</div>
		</div>
		<div class="min-w-0 rounded-md border border-base-300/50 bg-base-100/20 px-2 py-1.5">
			<div class="truncate opacity-60">{m.gpio_state({ locale: i18n.languageTag })}</div>
			<div class="truncate font-bold {status?.stable ? 'text-success' : 'text-warning'}">
				{status?.stable
					? m.gpio_stable({ locale: i18n.languageTag })
					: m.gpio_unstable({ locale: i18n.languageTag })}
			</div>
		</div>
	</div>

	<div class="grid grid-cols-1 gap-2 sm:grid-cols-2 lg:grid-cols-4">
		<FormSelect
			label={m.gpio_mode({ locale: i18n.languageTag })}
			value={channel.mode}
			options={modeOptions}
			size="xs"
			disabled={controlsDisabled}
			onchange={handleModeChange}
		/>

		{#if channel.mode === 'input'}
			<FormSelect
				label={m.gpio_pull({ locale: i18n.languageTag })}
				value={channel.pull}
				options={pullOptions}
				size="xs"
				disabled={controlsDisabled}
				onchange={handlePullChange}
			/>
			<FormInput
				label={m.gpio_debounce({ locale: i18n.languageTag })}
				type="number"
				value={channel.debounce_ms}
				min={GPIO_DEBOUNCE_MIN_MS}
				max={GPIO_DEBOUNCE_MAX_MS}
				step={10}
				sizeVariant="xs"
				disabled={controlsDisabled}
				oninput={handleDebounceInput}
			/>
		{/if}

		{#if channel.mode === 'output'}
			<FormToggle
				label={m.gpio_output_level({ locale: i18n.languageTag })}
				checked={logicalValue}
				disabled={outputDisabled}
				onchange={handleOutputChange}
				plain
				class="flex min-h-12 items-center justify-between gap-2 rounded-md border border-base-300/60 bg-base-100/30 px-3 py-2"
			/>
		{/if}

		{#if channel.mode !== 'disabled'}
			<FormToggle
				label={m.gpio_inverted({ locale: i18n.languageTag })}
				checked={channel.inverted}
				disabled={controlsDisabled}
				onchange={handleInvertChange}
				plain
				class="flex min-h-12 items-center justify-between gap-2 rounded-md border border-base-300/60 bg-base-100/30 px-3 py-2"
			/>
		{/if}
	</div>

	<div class="flex items-center justify-end gap-2">
		<div class="hidden h-9 w-5 items-center justify-center md:flex">
			<Circle
				class={status?.configured && status.logical ? 'h-3 w-3 text-success' : 'h-3 w-3 opacity-20'}
			/>
		</div>
		<FormButton
			label=""
			icon={Power}
			variant="ghost"
			size="sm"
			class="btn-square"
			disabled={controlsDisabled}
			onclick={handleDisable}
			ariaLabel={m.gpio_disable_channel({ locale: i18n.languageTag })}
			title={m.gpio_disable_channel({ locale: i18n.languageTag })}
		/>
	</div>
</ContentBox>
