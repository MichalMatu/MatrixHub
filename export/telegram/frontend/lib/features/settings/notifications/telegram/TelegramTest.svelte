<script lang="ts">
	import Send from '~icons/tabler/send';
	import Telegram from '~icons/tabler/brand-telegram';
	import WorkerStats from '$lib/components/notifications/WorkerStats.svelte';
	import { useTelegramTest } from './useTelegramTest.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	const testState = useTelegramTest();

	function handleSendTest() {
		void testState.sendTest();
	}
</script>

<BaseCard
	title={m.telegram_test_title({ locale: i18n.languageTag })}
	icon={Telegram}
	class="h-full"
>
	<ContentBox>
		<label class="form-control w-full">
			<div class="label pt-0">
				<span class="label-text font-bold"
					>{m.telegram_test_message({ locale: i18n.languageTag })}</span
				>
				<span class="label-text-alt opacity-70"
					>{m.telegram_test_message_max({ locale: i18n.languageTag })}</span
				>
			</div>
			<textarea
				class="textarea textarea-bordered w-full"
				rows="3"
				maxlength="1024"
				placeholder={m.telegram_test_placeholder({ locale: i18n.languageTag })}
				bind:value={testState.testText}
				disabled={testState.sending}
			></textarea>
		</label>
	</ContentBox>

	<div class="flex items-center gap-3 mt-4">
		{#if testState.lastError}
			<span class="text-sm text-error flex-1">{testState.lastError}</span>
		{:else}
			<div class="flex-1"></div>
		{/if}
		<FormButton
			class="btn-primary"
			disabled={testState.blocked}
			onclick={handleSendTest}
			loading={testState.sending}
			icon={Send}
			label={testState.sending
				? m.telegram_test_sending({ locale: i18n.languageTag })
				: m.telegram_test_btn_send({ locale: i18n.languageTag })}
		/>
	</div>

	<WorkerStats type="telegram" />

	<div class="mt-4">
		<ContentBox>
			<div class="text-sm opacity-70 leading-relaxed space-y-1">
				<p>
					<strong>{m.telegram_test_note_title({ locale: i18n.languageTag })}</strong>
					{m.telegram_test_note_desc({ locale: i18n.languageTag })}
				</p>
				<ul class="list-disc ml-5 space-y-0.5">
					<li>{m.telegram_test_req_1({ locale: i18n.languageTag })}</li>
					<li>{m.telegram_test_req_2({ locale: i18n.languageTag })}</li>
					<li>{m.telegram_test_req_3({ locale: i18n.languageTag })}</li>
				</ul>
			</div>
		</ContentBox>
	</div>
</BaseCard>
