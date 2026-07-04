<script lang="ts">
	import Modal from '$lib/components/Modal.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import DataTable from '$lib/components/shared/tables/DataTable.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let {
		isOpen = $bindable()
	}: {
		isOpen: boolean;
	} = $props();

	const commands = [
		{ syntax: 'STRING [text]', desc: () => m.macros_cmd_string() },
		{ syntax: 'STRINGLN [text]', desc: () => m.macros_cmd_stringln() },
		{ syntax: 'DELAY [0-60000 ms]', desc: () => m.macros_cmd_delay() },
		{ syntax: 'DEFAULT_DELAY [0-60000 ms]', desc: () => m.macros_cmd_defdelay() },
		{ syntax: 'REPEAT [1-1000]', desc: () => m.macros_cmd_repeat() },
		{
			syntax: 'KEY [code]',
			desc: () => m.macros_cmd_key()
		},
		{ syntax: 'REM [comment]', desc: () => m.macros_cmd_rem() },
		{ syntax: 'CTRL [key]', desc: () => m.macros_cmd_ctrl() },
		{ syntax: 'ALT [key]', desc: () => m.macros_cmd_alt() },
		{ syntax: 'SHIFT [key]', desc: () => m.macros_cmd_shift() },
		{ syntax: 'GUI [key]', desc: () => m.macros_cmd_gui() },
		{ syntax: 'PRESS [key]', desc: () => m.macros_cmd_press() },
		{ syntax: 'RELEASE [key]', desc: () => m.macros_cmd_release() },
		{ syntax: 'RELEASE ALL', desc: () => m.macros_cmd_release_all() },
		{ syntax: 'CTRL ALT [key]', desc: () => m.macros_cmd_multi_mod() },
		{ syntax: 'MOUSE_MOVE [x] [y]', desc: () => m.macros_cmd_mouse_move() },
		{ syntax: 'MOUSE_CLICK [btn]', desc: () => m.macros_cmd_mouse_click() },
		{ syntax: 'MATRIX_PRINT "[text]"', desc: () => m.macros_cmd_matrix() },
		{ syntax: 'SYSTEM_SLEEP', desc: () => m.macros_cmd_system_sleep() },
		{ syntax: 'SYSTEM_WAKE', desc: () => m.macros_cmd_system_wake() },
		{ syntax: 'SYSTEM_POWER_OFF', desc: () => m.macros_cmd_system_power() },
		{ syntax: 'MEDIA [key] (or 0xCode)', desc: () => m.macros_cmd_media() },
		{ syntax: 'KEY 0x[code]', desc: () => m.macros_cmd_key_hex() }
	];
</script>

<Modal
	{isOpen}
	onClose={() => (isOpen = false)}
	title={m.macros_help_title()}
	widthClass="max-w-2xl"
>
	<div class="overflow-x-auto">
		<DataTable size="md">
			<thead>
				<tr>
					<th class="w-1/3">{m.macros_help_syntax()}</th>
					<th>{m.macros_help_desc()}</th>
				</tr>
			</thead>
			<tbody>
				{#each commands as cmd}
					<tr>
						<td class="font-mono text-xs md:text-sm bg-base-200/50 rounded p-2">
							{cmd.syntax}
						</td>
						<td class="text-xs md:text-sm">{cmd.desc()}</td>
					</tr>
				{/each}
			</tbody>
		</DataTable>
	</div>

	{#snippet actions()}
		<div class="flex justify-end w-full">
			<FormButton variant="neutral" label={m.action_close()} onclick={() => (isOpen = false)} />
		</div>
	{/snippet}
</Modal>
