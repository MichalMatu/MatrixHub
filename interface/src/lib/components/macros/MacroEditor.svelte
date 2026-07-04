<script lang="ts">
	import { FormInput, FormTextarea } from '$lib/components/shared/forms';
	import * as m from '$lib/paraglide/messages.js';

	let {
		filename = $bindable(''),
		content = $bindable(''),
		isNew = false
	}: {
		filename?: string;
		content?: string;
		isNew?: boolean;
	} = $props();
</script>

<div class="flex flex-col gap-4">
	<FormInput
		id="script-name"
		type="text"
		label={m.macros_editor_name()}
		placeholder={m.macros_editor_name_placeholder()}
		bind:value={filename}
		disabled={!isNew}
		maxlength={32}
	/>

	<FormTextarea
		id="script-content"
		label={m.macros_editor_content_label()}
		placeholder={m.macros_editor_content_placeholder()}
		bind:value={content}
		rows={10}
		class="font-mono text-sm leading-relaxed"
	/>
</div>
