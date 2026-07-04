<script lang="ts">
	import { FormInput } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { MAX_ALARM_NAME_LENGTH } from '$lib/types/domain/alarms';

	let {
		name = $bindable(),
		maxlength,
		placeholder
	}: { name: string; maxlength?: number; placeholder?: string } = $props();

	function filterAsciiPrintable(value: string): string {
		// Keep only printable ASCII (0x20..0x7E)
		return value.replace(/[^\x20-\x7E]/g, '');
	}
</script>

<div class="w-full">
	<label for="alarm-name" class="text-xs font-bold block opacity-70 uppercase tracking-wide mb-1">
		{m.alarms_field_name({ locale: i18n.languageTag })}
	</label>
	<FormInput
		id="alarm-name"
		bind:value={name}
		maxlength={maxlength ?? MAX_ALARM_NAME_LENGTH}
		placeholder={placeholder ?? m.alarms_field_name_placeholder({ locale: i18n.languageTag })}
		oninput={(e) => {
			name = filterAsciiPrintable((e.target as HTMLInputElement).value);
		}}
	/>
</div>
