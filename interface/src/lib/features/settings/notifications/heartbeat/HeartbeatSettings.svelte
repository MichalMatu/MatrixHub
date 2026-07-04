<script lang="ts">
	import Heart from '~icons/tabler/heart-rate-monitor';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { Spinner } from '$lib/components';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormToggle, FormInput } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	let { heartbeatState } = $props<{
		heartbeatState: ReturnType<typeof import('./useHeartbeatSettings.svelte').useHeartbeatSettings>;
	}>();

	function handleSubmit(event: Event) {
		event.preventDefault();
		heartbeatState.saveSettings();
	}

	function handleSave() {
		heartbeatState.saveSettings();
	}

	function handleUrlNormalize(event: Event, index: number) {
		const input = event.target as HTMLInputElement;
		heartbeatState.setSlotUrl(index, input.value);
	}

	function handleUrlInput(event: Event, index: number) {
		const input = event.target as HTMLInputElement;
		heartbeatState.setSlotUrlRaw(index, input.value);
	}

	function handleNameInput(event: Event, index: number) {
		const input = event.target as HTMLInputElement;
		heartbeatState.setSlotName(index, input.value);
	}

	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.heartbeat_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.heartbeat_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.heartbeat_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/alarms', label: m.menu_alarms({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<SettingsCard
	title={m.heartbeat_title({ locale: i18n.languageTag })}
	icon={Heart}
	hasChanges={heartbeatState.hasChanges}
	loading={heartbeatState.loading}
	saving={heartbeatState.saving}
	onSave={handleSave}
	onReset={heartbeatState.resetSettings}
	dirtySourceId="heartbeat-settings"
>
	{#snippet actions()}
		<HelpTriggerButton
			label={m.heartbeat_help_title({ locale })}
			onclick={() => (helpOpen = true)}
		/>
	{/snippet}

	{#if heartbeatState.loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<form onsubmit={handleSubmit} class="flex flex-col gap-1">
			{#each heartbeatState.settings.slots as slot, index (index)}
				<ContentBox paddingClass="p-3">
					<div class="flex items-center gap-2 mb-2">
						<FormToggle
							bind:checked={slot.enabled}
							label={''}
							ariaLabel={m.heartbeat_enable_slot(
								{ index: index + 1 },
								{ locale: i18n.languageTag }
							)}
							class="p-0 min-h-0"
							plain={true}
						/>
						<div class="flex-1">
							<FormInput
								id="slot_{index}_name"
								bind:value={slot.name}
								oninput={(e) => handleNameInput(e, index)}
								placeholder={m.heartbeat_name_placeholder({ locale: i18n.languageTag })}
								ariaLabel={m.heartbeat_slot_name(
									{ index: index + 1 },
									{ locale: i18n.languageTag }
								)}
								maxlength={23}
								label={''}
							/>
						</div>
					</div>
					<FormInput
						id="slot_{index}_url"
						type="text"
						bind:value={slot.url}
						error={slot.enabled && !slot.url
							? m.heartbeat_url_required({ locale: i18n.languageTag })
							: undefined}
						onpaste={(e) => handleUrlNormalize(e, index)}
						onblur={(e) => handleUrlNormalize(e, index)}
						oninput={(e) => handleUrlInput(e, index)}
						placeholder={m.heartbeat_url_placeholder({ locale: i18n.languageTag })}
						ariaLabel={m.heartbeat_slot_url({ index: index + 1 }, { locale: i18n.languageTag })}
						maxlength={190}
						label={''}
					/>
					{#if slot.url.trim().startsWith('https://')}
						<div class="mt-2 rounded-box bg-base-200/60 px-3 py-2">
							<FormToggle
								bind:checked={slot.allow_insecure}
								onchange={() => heartbeatState.setSlotAllowInsecure(index, slot.allow_insecure)}
								label={m.heartbeat_allow_insecure_label({ locale: i18n.languageTag })}
								description={m.heartbeat_allow_insecure_desc({ locale: i18n.languageTag })}
								plain={true}
								class="flex items-center justify-between gap-3"
							/>
						</div>
					{/if}
				</ContentBox>
			{/each}
		</form>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.heartbeat_help_title({ locale })}
		intro={m.heartbeat_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
