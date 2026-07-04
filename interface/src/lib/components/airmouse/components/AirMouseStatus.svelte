<script lang="ts">
	import { fade } from 'svelte/transition';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import type { AirMouseState } from '../airMouseState.svelte';
	import {
		AIR_MOUSE_CLICK_ACTION,
		AIR_MOUSE_CLICK_SOURCE,
		AIR_MOUSE_DEFAULTS
	} from '../airMouseConfig';
	import { createAirMouseGyroPolylinePoints, toAirMousePercent } from '../airMouseStatusModel';

	let { mouseState }: { mouseState: AirMouseState } = $props();

	const DEFAULT_TAP_THRESHOLD_LABEL = AIR_MOUSE_DEFAULTS.tapThreshold.toFixed(1);

	let thresholdPercent = $derived(
		mouseState.status ? toAirMousePercent(mouseState.status.tap_threshold_g) : 50
	);

	let currentGPercent = $derived(
		mouseState.status ? toAirMousePercent(mouseState.status.last_delta_g) : 0
	);

	const clickSource = $derived(mouseState.status?.click_source ?? AIR_MOUSE_CLICK_SOURCE.SENSOR);
	const isTapSource = $derived(clickSource === AIR_MOUSE_CLICK_SOURCE.SENSOR);
	const clickSourceLabel = $derived(
		clickSource === AIR_MOUSE_CLICK_SOURCE.SENSOR
			? m.airmouse_click_source_sensor()
			: m.airmouse_click_source_button()
	);

	function getActionLabel(action: number, script?: string) {
		switch (action) {
			case AIR_MOUSE_CLICK_ACTION.LEFT:
				return m.airmouse_action_left_click();
			case AIR_MOUSE_CLICK_ACTION.RIGHT:
				return m.airmouse_action_right_click();
			case AIR_MOUSE_CLICK_ACTION.MIDDLE:
				return m.airmouse_action_middle_click();
			case AIR_MOUSE_CLICK_ACTION.SCRIPT:
				return script
					? `${m.airmouse_action_run_script()}: ${script}`
					: m.airmouse_action_run_script();
			default:
				return m.airmouse_action_none();
		}
	}

	const singleActionLabel = $derived(
		getActionLabel(
			mouseState.status?.single_click_action ?? 0,
			mouseState.status?.single_click_script
		)
	);
	const doubleActionLabel = $derived(
		getActionLabel(
			mouseState.status?.double_click_action ?? 0,
			mouseState.status?.double_click_script
		)
	);
	const tripleActionLabel = $derived(
		getActionLabel(
			mouseState.status?.triple_click_action ?? 0,
			mouseState.status?.triple_click_script
		)
	);

	let pointsX = $derived(
		createAirMouseGyroPolylinePoints(mouseState.gyroXHistory, mouseState.maxHistory)
	);
	let pointsZ = $derived(
		createAirMouseGyroPolylinePoints(mouseState.gyroZHistory, mouseState.maxHistory)
	);
</script>

<div class="grid grid-cols-1 sm:grid-cols-3 gap-1 items-stretch" in:fade>
	<!-- Delta G Stats -->
	<ContentBox paddingClass="p-3" class="flex flex-col justify-center">
		<div class="flex justify-between items-center">
			<span class="text-xs opacity-70">{m.airmouse_delta_g_label()}</span>
			<span
				class="font-mono font-bold text-lg"
				class:text-error={mouseState.status &&
					mouseState.status.last_delta_g > mouseState.status.tap_threshold_g}
			>
				{mouseState.status?.last_delta_g?.toFixed(2) ?? '0.00'}
			</span>
		</div>
		<div class="flex justify-between items-center mt-1 pt-1 border-t border-base-content/10">
			<span class="text-[10px] opacity-50">{m.airmouse_peak()}</span>
			<span class="font-mono text-sm opacity-80">{mouseState.maxDeltaG.toFixed(2)}</span>
		</div>
	</ContentBox>

	<!-- Gyro XYZ -->
	<ContentBox class="flex flex-col justify-center gap-0.5">
		<div class="flex justify-between text-xs">
			<span class="text-red-400 font-bold">{m.airmouse_axis_x()}</span>
			<span class="font-mono">{mouseState.status?.imu?.gx?.toFixed(0) ?? '0'}</span>
		</div>
		<div class="flex justify-between text-xs">
			<span class="text-green-400 font-bold">{m.airmouse_axis_y()}</span>
			<span class="font-mono">{mouseState.status?.imu?.gy?.toFixed(0) ?? '0'}</span>
		</div>
		<div class="flex justify-between text-xs">
			<span class="text-blue-400 font-bold">{m.airmouse_axis_z()}</span>
			<span class="font-mono">{mouseState.status?.imu?.gz?.toFixed(0) ?? '0'}</span>
		</div>
	</ContentBox>

	<!-- Tap Force Bar -->
	<ContentBox paddingClass="p-3" class="flex flex-col justify-center">
		<div class="flex justify-between text-[10px] opacity-70 mb-1">
			<span class="opacity-60">{m.airmouse_click_source()}</span>
			<span class="truncate max-w-[140px] text-right">{clickSourceLabel}</span>
		</div>

		{#if isTapSource}
			<div class="flex justify-between text-[10px] opacity-70">
				<span class="opacity-60">{m.airmouse_tap_threshold()}</span>
				<span>{mouseState.status?.tap_threshold_g?.toFixed(1) ?? DEFAULT_TAP_THRESHOLD_LABEL}G</span
				>
			</div>
			<div class="flex justify-between text-[10px] opacity-70 mb-1">
				<span class="opacity-60">{m.airmouse_debounce()}</span>
				<span>{mouseState.status?.click_debounce_ms ?? AIR_MOUSE_DEFAULTS.clickDebounce}ms</span>
			</div>
			<div class="relative h-4 bg-base-100 rounded overflow-hidden">
				<div
					class="absolute top-0 bottom-0 w-0.5 bg-info z-10"
					style="left: {thresholdPercent}%"
				></div>
				<div
					class="absolute top-0 bottom-0 left-0 transition-all duration-75"
					class:bg-success={currentGPercent < thresholdPercent * 0.7}
					class:bg-warning={currentGPercent >= thresholdPercent * 0.7 &&
						currentGPercent < thresholdPercent}
					class:bg-error={currentGPercent >= thresholdPercent}
					style="width: {currentGPercent}%"
				></div>
			</div>
		{:else}
			<div class="flex flex-col gap-1 text-[10px]">
				<div class="flex justify-between gap-2">
					<span class="opacity-60">{m.airmouse_single_click_action()}</span>
					<span class="text-right truncate">{singleActionLabel}</span>
				</div>
				<div class="flex justify-between gap-2">
					<span class="opacity-60">{m.airmouse_double_click_action()}</span>
					<span class="text-right truncate">{doubleActionLabel}</span>
				</div>
				<div class="flex justify-between gap-2">
					<span class="opacity-60">{m.airmouse_triple_click_action()}</span>
					<span class="text-right truncate">{tripleActionLabel}</span>
				</div>
				<div class="flex justify-between gap-2 opacity-70">
					<span class="opacity-60">{m.airmouse_double_click()}</span>
					<span
						>{mouseState.status?.double_click_window_ms ??
							AIR_MOUSE_DEFAULTS.doubleClickWindow}ms</span
					>
				</div>
			</div>
		{/if}
	</ContentBox>
</div>

<!-- Gyroscope Chart -->
<div in:fade class="mt-1">
	<ContentBox>
		<div class="flex justify-between items-center mb-1">
			<div class="text-xs opacity-50">{m.airmouse_gyro_chart()}</div>
			<div class="flex gap-3 text-[10px] font-bold uppercase tracking-wider opacity-60">
				<div class="flex items-center gap-1">
					<span class="w-3 h-0.5 bg-error rounded-full shadow-[0_0_4px_currentColor]"></span>
					{m.airmouse_gyro_pitch_label()}
				</div>
				<div class="flex items-center gap-1">
					<span class="w-3 h-0.5 bg-info rounded-full shadow-[0_0_4px_currentColor]"></span>
					{m.airmouse_gyro_yaw_label()}
				</div>
			</div>
		</div>
		<div
			class="h-16 bg-base-300 rounded-lg overflow-hidden relative border border-base-content/5 shadow-inner"
		>
			<!-- Center line -->
			<div class="absolute top-1/2 left-0 right-0 h-px bg-base-content opacity-20"></div>
			<!-- SVG Chart -->
			<svg
				viewBox="0 -50 100 100"
				preserveAspectRatio="none"
				class="w-full h-full absolute inset-0"
			>
				<polyline
					points={pointsX}
					fill="none"
					class="stroke-error drop-shadow-sm"
					stroke-width="1.5"
					stroke-linecap="round"
					stroke-linejoin="round"
				/>
				<polyline
					points={pointsZ}
					fill="none"
					class="stroke-info drop-shadow-sm"
					stroke-width="1.5"
					stroke-linecap="round"
					stroke-linejoin="round"
				/>
			</svg>
		</div>
	</ContentBox>
</div>
