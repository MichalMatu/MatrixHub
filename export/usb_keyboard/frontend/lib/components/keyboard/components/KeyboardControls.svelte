<script lang="ts">
	import * as m from '$lib/paraglide/messages';
	import { FormButton } from '$lib/components/shared/forms';

	interface Props {
		textQueue: string;
		sending: boolean;
		autoSend: boolean;
		captureMode: boolean;
		languageName: string;
		placeholder?: string;
		onSend: () => void;
		onLiveToggle: () => void;
		onCaptureToggle: () => void;
		onLanguageToggle: () => void;
		onKeydown: (e: KeyboardEvent) => void;
	}

	let {
		textQueue = $bindable(),
		sending,
		autoSend,
		captureMode,
		languageName,
		placeholder = '',
		onSend,
		onLiveToggle,
		onCaptureToggle,
		onLanguageToggle,
		onKeydown
	}: Props = $props();
</script>

<div class="flex gap-2 items-stretch">
	<textarea
		class="textarea textarea-bordered flex-1 font-mono resize-none text-base h-20"
		bind:value={textQueue}
		onkeydown={onKeydown}
		disabled={captureMode}
		{placeholder}
	></textarea>

	<div class="grid grid-cols-2 grid-rows-2 gap-2 min-w-[168px] w-[168px]">
		<FormButton
			label={sending ? '...' : m.keyboard_send()}
			class="btn-success font-bold p-0 min-h-0 h-auto text-sm"
			onclick={onSend}
			disabled={sending || !textQueue}
		/>

		<button
			type="button"
			class="flex items-center justify-center p-0 min-h-0 h-auto text-sm relative font-medium transition-all select-none cursor-pointer live-btn {autoSend
				? 'live-active'
				: ''}"
			onclick={onLiveToggle}
			aria-pressed={autoSend}
			aria-label={m.keyboard_toggle_live_mode()}
			title={m.keyboard_toggle_live_mode()}
		>
			<span class="text-[10px] uppercase opacity-90">{m.keyboard_live()}</span>
		</button>

		<button
			type="button"
			class="flex items-center justify-center p-0 min-h-0 h-auto text-sm relative font-medium transition-all select-none cursor-pointer live-btn {captureMode
				? 'live-active'
				: ''}"
			onclick={onCaptureToggle}
			aria-pressed={captureMode}
			aria-label={m.keyboard_toggle_capture_mode()}
			title={m.keyboard_toggle_capture_mode()}
		>
			<span class="text-[10px] uppercase opacity-90">{m.keyboard_capture()}</span>
		</button>

		<button
			type="button"
			class="flex items-center justify-center p-0 min-h-0 h-auto text-sm relative font-medium transition-all select-none cursor-pointer live-btn"
			onclick={onLanguageToggle}
			aria-label={m.keyboard_switch_layout()}
			title={m.keyboard_switch_layout()}
		>
			<span class="text-[10px] uppercase opacity-90">{languageName}</span>
		</button>
	</div>
</div>

<style>
	/* Active Indicator (Green Dot - MacBook Style) */
	/* Shared dot style for live button */
	.live-btn.live-active::after {
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

	/* Live Button Custom Style */
	.live-btn {
		background: rgba(255, 255, 255, 0.08);
		color: white;
		border-bottom: 2px solid rgba(0, 0, 0, 0.5);
		border-radius: 6px;
	}
	.live-btn:hover {
		background: rgba(255, 255, 255, 0.15);
	}
	.live-btn:active {
		transform: translateY(2px);
		background: rgba(255, 255, 255, 0.2);
	}
</style>
