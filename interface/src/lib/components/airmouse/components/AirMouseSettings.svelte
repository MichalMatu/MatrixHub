<script lang="ts">
	import { FormButton, FormRange, FormToggle, FormSelect } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import Pointer from '~icons/tabler/pointer';
	import HandClick from '~icons/tabler/hand-click';
	import Target from '~icons/tabler/target';
	import * as m from '$lib/paraglide/messages.js';
	import type { AirMouseState } from '../airMouseState.svelte';
	import {
		AIR_MOUSE_CLICK_ACTION,
		AIR_MOUSE_CLICK_SOURCE,
		AIR_MOUSE_DEFAULTS,
		AIR_MOUSE_LIMITS,
		type AirMouseSettingsDefaults
	} from '../airMouseConfig';
	import {
		AIR_MOUSE_CLICK_BINDINGS,
		applyCombinedActionValue,
		createAirMouseActionOptions,
		createAirMouseSourceOptions,
		getCombinedActionValue
	} from '../airMouseSettingsModel';

	// We accept a flat config object that mirrors the structure needed for UI
	// Parent handles constructing/destructing

	interface Props {
		settings?: AirMouseSettingsDefaults;
		scripts?: Array<{ name: string }>;
		mouseState?: AirMouseState;
		section?: 'cursor' | 'click' | 'all';
		showHeader?: boolean;
	}

	let {
		settings = $bindable({ ...AIR_MOUSE_DEFAULTS }),
		scripts = [],
		mouseState,
		section = 'all',
		showHeader = true
	}: Props = $props();

	const showCursor = $derived(section !== 'click');
	const showClick = $derived(section !== 'cursor');

	const actionOptions = $derived(
		createAirMouseActionOptions(scripts, settings, {
			actionNone: m.airmouse_action_none(),
			actionLeft: m.airmouse_action_left_click(),
			actionRight: m.airmouse_action_right_click(),
			actionMiddle: m.airmouse_action_middle_click(),
			scriptsSeparator: m.airmouse_scripts_separator(),
			missingScriptSuffix: m.airmouse_script_missing(),
			sourceSensor: m.airmouse_click_source_sensor(),
			sourceButton: m.airmouse_click_source_button()
		})
	);

	const sourceOptions = $derived(
		createAirMouseSourceOptions({
			sourceSensor: m.airmouse_click_source_sensor(),
			sourceButton: m.airmouse_click_source_button()
		})
	);

	const { sensitivity, deadzone, acceleration, tapThreshold, clickDebounce, doubleClickWindow } =
		AIR_MOUSE_LIMITS;
</script>

<div class="flex flex-col gap-1">
	<!-- Cursor Movement Settings -->
	{#if showCursor}
		<div class="flex flex-col gap-1">
			{#if showHeader}
				<div
					class="text-xs font-semibold uppercase tracking-wide opacity-60 flex items-center gap-2"
				>
					<Pointer class="w-4 h-4" />
					{m.airmouse_cursor_title()}
				</div>
			{/if}

			<FormToggle
				label={m.airmouse_movement_enabled()}
				description={settings.movement_enabled
					? m.airmouse_movement_desc_on()
					: m.airmouse_movement_desc_off()}
				bind:checked={settings.movement_enabled}
			/>

			{#if mouseState}
				<ContentBox>
					<FormButton
						label={mouseState.calibrating ? m.common_loading() : m.airmouse_recalibrate()}
						icon={Target}
						variant={mouseState.calibrating ? 'ghost' : 'primary'}
						size="sm"
						onclick={() => mouseState.calibrate()}
						disabled={!mouseState.status ||
							(!mouseState.status.movement_enabled &&
								!(
									mouseState.status.click_enabled &&
									mouseState.status.click_source === AIR_MOUSE_CLICK_SOURCE.SENSOR
								)) ||
							mouseState.calibrating}
						class="w-full"
					/>
				</ContentBox>
			{/if}

			<ContentBox>
				<FormRange
					label={m.airmouse_sens_x()}
					bind:value={settings.sensitivityX}
					min={sensitivity.min}
					max={sensitivity.max}
					step={sensitivity.step}
				/>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={m.airmouse_sens_y()}
					bind:value={settings.sensitivityY}
					min={sensitivity.min}
					max={sensitivity.max}
					step={sensitivity.step}
				/>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={m.airmouse_deadzone()}
					bind:value={settings.deadzone}
					min={deadzone.min}
					max={deadzone.max}
					step={deadzone.step}
				/>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={m.airmouse_acceleration()}
					bind:value={settings.accelerationFactor}
					min={acceleration.min}
					max={acceleration.max}
					step={acceleration.step}
				/>
			</ContentBox>
		</div>
	{/if}

	<!-- Click Settings -->
	{#if showClick}
		<div class="flex flex-col gap-1">
			{#if showHeader}
				<div
					class="text-xs font-semibold uppercase tracking-wide opacity-60 flex items-center gap-2"
				>
					<HandClick class="w-4 h-4" />
					{m.airmouse_click_title()}
				</div>
			{/if}

			<FormToggle
				label={m.airmouse_click_enabled()}
				description={settings.click_enabled
					? m.airmouse_click_desc_on()
					: m.airmouse_click_desc_off()}
				bind:checked={settings.click_enabled}
			/>

			<ContentBox>
				<FormSelect
					label={m.airmouse_click_source()}
					options={sourceOptions}
					bind:value={settings.click_source}
				/>
			</ContentBox>

			<!-- Click action mapping (applies to both sensor taps and physical button) -->
			{#each AIR_MOUSE_CLICK_BINDINGS as binding (binding.actionKey)}
				<ContentBox>
					<FormSelect
						label={binding.label === 'single'
							? m.airmouse_single_click_action()
							: binding.label === 'double'
								? m.airmouse_double_click_action()
								: m.airmouse_triple_click_action()}
						options={actionOptions}
						value={getCombinedActionValue(settings[binding.actionKey], settings[binding.scriptKey])}
						onchange={(e) =>
							applyCombinedActionValue(
								settings,
								(e.currentTarget as HTMLSelectElement).value,
								binding.actionKey,
								binding.scriptKey
							)}
					/>
					{#if mouseState?.macrosEnabled === false && settings[binding.actionKey] === AIR_MOUSE_CLICK_ACTION.SCRIPT}
						<div class="mt-2 text-xs text-warning">
							{m.airmouse_macro_requires_macros()}
							<a href="/usb-features/macros" class="link link-primary ml-1 font-medium">
								{m.airmouse_macro_open_macros()}
							</a>
						</div>
					{/if}
				</ContentBox>
			{/each}

			<!-- Sliders at the bottom, filtered by mode -->
			{#if settings.click_source === AIR_MOUSE_CLICK_SOURCE.SENSOR}
				<ContentBox>
					<FormRange
						label={m.airmouse_tap_threshold()}
						bind:value={settings.tapThreshold}
						min={tapThreshold.min}
						max={tapThreshold.max}
						step={tapThreshold.step}
						suffix="G"
					/>
				</ContentBox>
				<ContentBox>
					<FormRange
						label={m.airmouse_debounce()}
						bind:value={settings.clickDebounce}
						min={clickDebounce.min}
						max={clickDebounce.max}
						step={clickDebounce.step}
						suffix="ms"
					/>
				</ContentBox>
			{:else}
				<ContentBox>
					<FormRange
						label={m.airmouse_double_click()}
						bind:value={settings.doubleClickWindow}
						min={doubleClickWindow.min}
						max={doubleClickWindow.max}
						step={doubleClickWindow.step}
						suffix="ms"
					/>
				</ContentBox>
			{/if}
		</div>
	{/if}
</div>
