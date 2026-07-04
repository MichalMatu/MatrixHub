<script lang="ts">
	import Modal from '$lib/components/Modal.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import * as m from '$lib/paraglide/messages.js';

	let {
		isOpen = $bindable()
	}: {
		isOpen: boolean;
	} = $props();
</script>

<Modal
	{isOpen}
	onClose={() => (isOpen = false)}
	title={m.airmouse_click_help_title()}
	widthClass="max-w-2xl"
>
	<div class="flex flex-col gap-3 text-sm">
		<p class="opacity-80">{m.airmouse_click_help_intro()}</p>

		<div class="flex flex-col gap-2">
			<div>
				<span class="font-semibold">{m.airmouse_click_help_click_enabled_label()}</span>
				{m.airmouse_click_help_click_enabled_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_click_help_click_source_label()}</span>
				{m.airmouse_click_help_click_source_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_click_help_tap_threshold_label()}</span>
				{m.airmouse_click_help_tap_threshold_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_click_help_debounce_label()}</span>
				{m.airmouse_click_help_debounce_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_click_help_click_speed_label()}</span>
				{m.airmouse_click_help_click_speed_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_click_help_actions_label()}</span>
				{m.airmouse_click_help_actions_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_click_help_script_label()}</span>
				{m.airmouse_click_help_script_desc()}
			</div>
			<div class="opacity-70">{m.airmouse_click_help_note()}</div>
		</div>
	</div>

	{#snippet actions()}
		<div class="flex justify-end w-full">
			<FormButton variant="neutral" label={m.action_close()} onclick={() => (isOpen = false)} />
		</div>
	{/snippet}
</Modal>
