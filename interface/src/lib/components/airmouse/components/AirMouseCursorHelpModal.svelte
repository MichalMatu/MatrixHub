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
	title={m.airmouse_cursor_help_title()}
	widthClass="max-w-2xl"
>
	<div class="flex flex-col gap-3 text-sm">
		<p class="opacity-80">{m.airmouse_cursor_help_intro()}</p>

		<div class="flex flex-col gap-2">
			<div>
				<span class="font-semibold">{m.airmouse_cursor_help_movement_enabled_label()}</span>
				{m.airmouse_cursor_help_movement_enabled_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_cursor_help_sensitivity_label()}</span>
				{m.airmouse_cursor_help_sensitivity_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_cursor_help_deadzone_label()}</span>
				{m.airmouse_cursor_help_deadzone_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_cursor_help_acceleration_label()}</span>
				{m.airmouse_cursor_help_acceleration_desc()}
			</div>
			<div>
				<span class="font-semibold">{m.airmouse_cursor_help_calibrate_label()}</span>
				{m.airmouse_cursor_help_calibrate_desc()}
			</div>
			<div class="opacity-70">{m.airmouse_cursor_help_note()}</div>
		</div>
	</div>

	{#snippet actions()}
		<div class="flex justify-end w-full">
			<FormButton variant="neutral" label={m.action_close()} onclick={() => (isOpen = false)} />
		</div>
	{/snippet}
</Modal>
