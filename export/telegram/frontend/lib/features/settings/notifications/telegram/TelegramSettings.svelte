<script lang="ts">
	import { Spinner } from '$lib/components';
	import Telegram from '~icons/tabler/brand-telegram';
	import Save from '~icons/tabler/device-floppy';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	import TelegramFormFields from './TelegramFormFields.svelte';
	import TelegramInstructions from './TelegramInstructions.svelte';

	let { telegramState } = $props<{
		telegramState: ReturnType<typeof import('./useTelegramSettings.svelte').useTelegramSettings>;
	}>();
	let formField: HTMLFormElement | undefined = $state();

	function handleSubmit() {
		if (!telegramState.hasChanges) return;
		telegramState.saveSettings();
	}

	function preventDefault(fn: () => void) {
		return function (event: Event) {
			event.preventDefault();
			fn();
		};
	}

	import { usePolling } from '$lib/utils/api/usePolling.svelte';
	import { isValidTelegramBotToken } from '$lib/utils';

	// Auto-poll for Chat ID when enabled but missing
	let needsPolling = $derived(
		telegramState.settings.enabled &&
			isValidTelegramBotToken(telegramState.settings.bot_token) &&
			!telegramState.settings.chat_id
	);

	const { start, stop } = usePolling(
		async () => {
			// Don't overwrite user changes or spam requests if busy
			if (!telegramState.hasChanges && !telegramState.loading && !telegramState.saving) {
				await telegramState.refreshSettings();
			}
		},
		{
			intervalMs: 5000,
			autoStart: false
		}
	);

	$effect(() => {
		if (needsPolling) {
			start();
		} else {
			stop();
		}
	});
</script>

<BaseCard title={m.telegram_title({ locale: i18n.languageTag })} icon={Telegram}>
	{#if telegramState.loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<div class="flex h-full flex-col gap-4">
			<form
				onsubmit={preventDefault(handleSubmit)}
				novalidate
				bind:this={formField}
				class="flex flex-1 flex-col gap-1"
			>
				<input type="text" name="username" autocomplete="username" class="hidden" tabindex="-1" />

				<TelegramFormFields
					settings={telegramState.settings}
					updateSetting={telegramState.updateSetting}
					formErrors={telegramState.errors}
				/>

				<TelegramInstructions />

				<div class="flex justify-end mt-4">
					<FormButton
						type="submit"
						label={m.action_save({ locale: i18n.languageTag })}
						icon={Save}
						loading={telegramState.saving}
						disabled={!telegramState.hasChanges}
					/>
				</div>
			</form>
		</div>
	{/if}
</BaseCard>
