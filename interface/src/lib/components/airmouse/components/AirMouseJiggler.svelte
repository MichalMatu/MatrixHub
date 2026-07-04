<script lang="ts">
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormSelect, FormToggle, FormRange } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import Activity from '~icons/tabler/activity';
	import * as m from '$lib/paraglide/messages.js';
	import type { AirMouseState } from '../airMouseState.svelte';
	import { fade } from 'svelte/transition';
	import { AIR_MOUSE_JIGGLER_LIMITS, AIR_MOUSE_JIGGLER_MODES } from '../airMouseConfig';
	import { useAirMouseJigglerForm } from '../useAirMouseJigglerForm.svelte';

	let {
		mouseState
	}: {
		mouseState: AirMouseState;
	} = $props();

	const form = useAirMouseJigglerForm(() => mouseState);

	const modeOptions = [
		{ value: AIR_MOUSE_JIGGLER_MODES.STEALTH, label: m.jiggler_mode_stealth() },
		{ value: AIR_MOUSE_JIGGLER_MODES.ACTIVE, label: m.jiggler_mode_active() },
		{ value: AIR_MOUSE_JIGGLER_MODES.HUMAN, label: m.jiggler_mode_bezier() },
		{ value: AIR_MOUSE_JIGGLER_MODES.KEYBOARD, label: m.jiggler_mode_keyboard() }
	];
</script>

<SettingsCard
	title={m.jiggler_title()}
	icon={Activity}
	class="h-full"
	hideTitleOnTiny={false}
	hasChanges={form.hasChanges}
	saving={form.saving}
	onSave={form.confirmSave}
	onReset={form.reset}
	dirtySourceId="airmouse-jiggler-settings"
>
	<div class="flex flex-col gap-1" in:fade>
		<FormToggle
			label={m.jiggler_title()}
			description={form.settings.enabled ? m.jiggler_toggle_desc_on() : m.jiggler_toggle_desc_off()}
			bind:checked={form.settings.enabled}
			disabled={form.saving}
		/>

		<div class="flex flex-col gap-1">
			<ContentBox>
				<FormSelect
					label={m.jiggler_mode()}
					bind:value={form.settings.mode}
					options={modeOptions}
				/>
				<div class="label py-0 mt-1">
					<span class="label-text-alt opacity-60 whitespace-normal break-words">
						{form.settings.mode === AIR_MOUSE_JIGGLER_MODES.STEALTH
							? m.jiggler_desc_stealth()
							: form.settings.mode === AIR_MOUSE_JIGGLER_MODES.ACTIVE
								? m.jiggler_desc_active()
								: form.settings.mode === AIR_MOUSE_JIGGLER_MODES.HUMAN
									? m.jiggler_desc_bezier()
									: form.settings.mode === AIR_MOUSE_JIGGLER_MODES.KEYBOARD
										? m.jiggler_desc_keyboard()
										: m.jiggler_desc_disabled()}
					</span>
				</div>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={m.jiggler_interval()}
					bind:value={form.settings.interval}
					min={AIR_MOUSE_JIGGLER_LIMITS.interval.min}
					max={AIR_MOUSE_JIGGLER_LIMITS.interval.max}
					step={AIR_MOUSE_JIGGLER_LIMITS.interval.step}
					suffix="s"
					valueClass="w-10"
				/>
			</ContentBox>

			<ContentBox>
				<div class="form-control">
					<label class="label cursor-pointer w-full items-center justify-between gap-3">
						<span class="label-text whitespace-normal flex-1 min-w-[150px]">
							{m.jiggler_random()}
						</span>
						<input
							type="checkbox"
							class="toggle toggle-sm toggle-primary shrink-0"
							bind:checked={form.settings.random}
						/>
					</label>
				</div>
			</ContentBox>

			{#if form.settings.mode !== AIR_MOUSE_JIGGLER_MODES.KEYBOARD}
				<div in:fade>
					<ContentBox>
						<FormRange
							label={m.jiggler_distance()}
							bind:value={form.settings.distance}
							min={AIR_MOUSE_JIGGLER_LIMITS.distance.min}
							max={AIR_MOUSE_JIGGLER_LIMITS.distance.max}
							step={AIR_MOUSE_JIGGLER_LIMITS.distance.step}
							suffix={m.jiggler_counts()}
							valueClass="w-12"
						/>
					</ContentBox>
				</div>
			{/if}
		</div>
	</div>
</SettingsCard>
