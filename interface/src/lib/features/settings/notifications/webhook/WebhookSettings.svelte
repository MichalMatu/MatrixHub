<script lang="ts">
	import { Spinner } from '$lib/components';
	import AP from '~icons/tabler/access-point';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormToggle, FormInput } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let { webhookState } = $props<{
		webhookState: ReturnType<typeof import('./useWebhookSettings.svelte').useWebhookSettings>;
	}>();

	function handleSubmit(event: Event) {
		event.preventDefault();
		if (!webhookState.hasChanges) return;
		webhookState.saveSettings();
	}

	function handleSave() {
		if (!webhookState.hasChanges) return;
		webhookState.saveSettings();
	}
</script>

<SettingsCard
	title={m.webhook_title({ locale: i18n.languageTag })}
	icon={AP}
	hasChanges={webhookState.hasChanges}
	loading={webhookState.loading}
	saving={webhookState.saving}
	onSave={handleSave}
	onReset={webhookState.resetSettings}
	dirtySourceId="webhook-settings"
>
	{#if webhookState.loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<form onsubmit={handleSubmit} class="flex flex-col gap-1" novalidate>
			<FormToggle
				label={m.webhook_enable({ locale: i18n.languageTag })}
				description={webhookState.settings.enabled
					? m.webhook_enabled_desc({ locale: i18n.languageTag })
					: m.webhook_disabled_desc({ locale: i18n.languageTag })}
				bind:checked={webhookState.settings.enabled}
			/>

			<ContentBox>
				<p class="text-base-content/70 text-sm mb-3">
					{m.webhook_desc({ locale: i18n.languageTag })}
				</p>

				<div class="mb-4">
					<details class="text-sm">
						<summary class="cursor-pointer text-sm font-medium opacity-80"
							>{m.webhook_compatible_services({ locale: i18n.languageTag })}</summary
						>
						<div class="mt-2 text-xs opacity-70 space-y-1 ml-2">
							<p class="mb-2">
								{m.webhook_compatible_hint({ locale: i18n.languageTag })}
							</p>
							<ul class="list-disc list-inside space-y-0.5 ml-1">
								<li>{m.webhook_compatible_service_chat({ locale: i18n.languageTag })}</li>
								<li>{m.webhook_compatible_service_automation({ locale: i18n.languageTag })}</li>
								<li>{m.webhook_compatible_service_home({ locale: i18n.languageTag })}</li>
								<li>{m.webhook_compatible_service_notifications({ locale: i18n.languageTag })}</li>
								<li>{m.webhook_compatible_service_custom({ locale: i18n.languageTag })}</li>
							</ul>
							<p class="mt-2">
								{m.webhook_discord_note({ locale: i18n.languageTag })}
							</p>
						</div>
					</details>
				</div>

				<div class="alert shadow-sm bg-base-100/50">
					<div class="text-xs opacity-70">
						{@html m.webhook_payload_hint(
							{ method: '<code class="bg-base-300 px-1 rounded font-mono">POST</code>' },
							{ locale: i18n.languageTag }
						)}
						<code class="bg-base-300 px-1 rounded mt-1 block font-mono"
							>&#123; "event": "alarm", ... &#125;</code
						>
					</div>
				</div>
			</ContentBox>

			<ContentBox>
				<FormInput
					label={m.webhook_url_label({ locale: i18n.languageTag })}
					id="webhook_url"
					type="url"
					value={webhookState.settings.url}
					oninput={(e) => {
						const val = (e.target as HTMLInputElement).value;
						webhookState.updateSetting('url', val);
					}}
					placeholder={m.webhook_url_placeholder({ locale: i18n.languageTag })}
					error={webhookState.errors.url
						? m.webhook_url_invalid({ locale: i18n.languageTag })
						: undefined}
					maxlength={255}
				/>
			</ContentBox>
		</form>
	{/if}
</SettingsCard>
