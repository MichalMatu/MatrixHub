<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { LAYOUT_CONFIG } from './config/layouts';
	import * as m from '$lib/paraglide/messages';

	import KeyboardControls from './components/KeyboardControls.svelte';
	import { useKeyboardManagement } from './useKeyboardManagement.svelte';

	let {
		show: _show = true,
		token = '',
		lastStatus = $bindable(''),
		lastStatusKind = $bindable<'success' | 'error' | 'info' | ''>('')
	} = $props();

	const controller = useKeyboardManagement(() => token);

	$effect(() => {
		lastStatus = controller.lastStatus;
		lastStatusKind = controller.lastStatusKind;
	});

	onMount(() => {
		controller.init();
	});

	onDestroy(() => {
		controller.destroy();
	});
</script>

<div class="rounded-box bg-base-100 p-3 flex flex-col gap-3 relative">
	<KeyboardControls
		bind:textQueue={controller.textQueue}
		sending={controller.sending}
		autoSend={controller.autoSend}
		captureMode={controller.captureMode}
		languageName={LAYOUT_CONFIG[controller.keyboardMode].name}
		placeholder={m.keyboard_placeholder()}
		onSend={() => controller.sendText()}
		onLiveToggle={() => controller.toggleLiveMode()}
		onCaptureToggle={() => controller.toggleCaptureMode()}
		onLanguageToggle={() => controller.toggleLanguage()}
		onKeydown={(e) => controller.handleKeydown(e)}
	/>

	<div class="overflow-x-auto w-full pb-2">
		<div class="min-w-[600px]">
			<div class="simple-keyboard text-black"></div>
		</div>
	</div>
</div>

<style>
	/* Dark Theme Overrides for Simple Keyboard */
	:global(.my-dark-theme) {
		background-color: transparent !important;
		color: white;
	}

	:global(.my-dark-theme .hg-button) {
		background: rgba(255, 255, 255, 0.08) !important;
		color: white !important;
		border-bottom: 2px solid rgba(0, 0, 0, 0.5) !important;
		border-top: none;
		border-left: none;
		border-right: none;
		border-radius: 6px;
		margin: 3px;
		height: 40px;
		display: flex;
		align-items: center;
		justify-content: center;
		font-weight: 500;
		position: relative; /* Needed for indicator */
	}

	:global(.my-dark-theme .hg-button:active) {
		background: rgba(255, 255, 255, 0.2) !important;
		transform: translateY(2px);
	}

	/* Special keys styling */
	:global(.hg-button[data-skbtnuid*='space']) {
		flex-grow: 4;
	}
	:global(.hg-button[data-skbtnuid*='enter']) {
		background: rgba(var(--color-good-rgb), 0.15) !important;
		color: var(--color-good) !important;
	}

	/* Active Indicator (Green Dot - MacBook Style) */
	/* Shared dot style for both keys and live button */
	:global(.key-active-dot)::after {
		content: '';
		position: absolute;
		top: 5px;
		left: 5px;
		width: 5px;
		height: 5px;
		background-color: var(--color-good, #4ade80);
		border-radius: 50%;
		box-shadow: 0 0 5px var(--color-good, #4ade80);
	}

	/* Keyboard Keys Specific Active Style (Bg + Border) */
	:global(.key-active-dot) {
		background: rgba(var(--color-good-rgb), 0.1) !important;
		border-color: var(--color-good) !important;
	}

	:global(.key-capture-pressed) {
		background: rgba(var(--color-good-rgb), 0.22) !important;
		border-bottom-color: rgba(var(--color-good-rgb), 0.8) !important;
		box-shadow: inset 0 0 0 1px rgba(var(--color-good-rgb), 0.35) !important;
		transform: translateY(2px);
	}
</style>
