<script lang="ts">
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton, FormTextarea } from '$lib/components/shared/forms';
	import Send from '~icons/tabler/send';
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';
	import WorkerStats from '$lib/components/notifications/WorkerStats.svelte';
	import { usePushoverTest } from './usePushoverTest.svelte';

	const testState = usePushoverTest();

	function handleSendTest() {
		void testState.sendTest();
	}
</script>

<BaseCard title={m.pushover_test_title({ locale: i18n.languageTag })} icon={Send} class="h-full">
	<ContentBox>
		<FormTextarea
			label={m.webhook_test_label({ locale: i18n.languageTag })}
			rows={3}
			maxlength={256}
			placeholder={m.pushover_test_placeholder({ locale: i18n.languageTag })}
			bind:value={testState.testText}
			disabled={testState.sending}
		/>
	</ContentBox>

	<div class="flex items-center gap-3 mt-4">
		{#if testState.lastError}
			<span class="text-sm text-error flex-1">{testState.lastError}</span>
		{:else}
			<div class="flex-1"></div>
		{/if}
		<FormButton
			class="btn-primary"
			disabled={testState.blocked || !testState.testText.trim()}
			onclick={handleSendTest}
			loading={testState.sending}
			icon={Send}
			label={testState.sending
				? m.telegram_test_sending({ locale: i18n.languageTag })
				: m.pushover_test_btn({ locale: i18n.languageTag })}
		/>
	</div>

	<WorkerStats type="pushover" />

	<ContentBox class="mt-4">
		<div class="text-sm opacity-70 leading-relaxed space-y-1">
			<p>
				<strong>{m.telegram_test_note_title({ locale: i18n.languageTag })}</strong>
				{m.telegram_test_note_desc({ locale: i18n.languageTag })}
			</p>
			<ul class="list-disc ml-5 space-y-0.5">
				<li>{m.pushover_test_req_1({ locale: i18n.languageTag })}</li>
				<li>{m.pushover_test_req_2({ locale: i18n.languageTag })}</li>
			</ul>
		</div>
	</ContentBox>
</BaseCard>
