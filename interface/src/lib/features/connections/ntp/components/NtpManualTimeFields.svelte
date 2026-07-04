<script lang="ts">
	import Clock from '~icons/tabler/clock';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	import { FormButton, FormInput } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	interface Props {
		manualTimeInput: string;
		onUseBrowserTime: () => void;
	}

	let { manualTimeInput = $bindable(), onUseBrowserTime }: Props = $props();

	function handleInput(event: Event) {
		const target = event.target as HTMLInputElement;
		manualTimeInput = target.value;
	}
</script>

<ContentBox>
	<div class="form-control">
		<label class="label py-0 mb-1" for="manual-time">
			<span class="label-text text-xs">{m.ntp_label_manual_time({ locale: i18n.languageTag })}</span
			>
		</label>
		<div class="flex gap-2 items-center">
			<FormInput
				id="manual-time"
				type="datetime-local"
				value={manualTimeInput}
				oninput={handleInput}
				step="1"
				class="w-full"
			/>
			<FormButton
				label=""
				icon={Clock}
				class="btn-square btn-sm"
				onclick={onUseBrowserTime}
				title={m.ntp_btn_browser_time({ locale: i18n.languageTag })}
			/>
		</div>
		<div class="text-xs opacity-75 mt-1 ml-1">
			{m.ntp_manual_time_hint({ locale: i18n.languageTag })}
		</div>
	</div>
</ContentBox>
