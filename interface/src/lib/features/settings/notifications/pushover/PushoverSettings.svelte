<script lang="ts">
	import { Spinner } from '$lib/components';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import Message from '~icons/tabler/message';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormToggle } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	import PushoverFormFields from './PushoverFormFields.svelte';

	let { pushoverState } = $props<{
		pushoverState: ReturnType<typeof import('./usePushoverSettings.svelte').usePushoverSettings>;
	}>();

	function handleSubmit(event: Event) {
		event.preventDefault();
		if (!pushoverState.hasChanges) return;
		pushoverState.saveSettings();
	}

	function handleSave() {
		if (!pushoverState.hasChanges) return;
		pushoverState.saveSettings();
	}

	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.pushover_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.pushover_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.pushover_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/alarms', label: m.menu_alarms({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<SettingsCard
	title={m.pushover_title({ locale: i18n.languageTag })}
	icon={Message}
	hasChanges={pushoverState.hasChanges}
	loading={pushoverState.loading}
	saving={pushoverState.saving}
	onSave={handleSave}
	onReset={pushoverState.resetSettings}
	dirtySourceId="pushover-settings"
>
	{#snippet actions()}
		<HelpTriggerButton
			label={m.pushover_help_title({ locale })}
			onclick={() => (helpOpen = true)}
		/>
	{/snippet}

	{#if pushoverState.loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<form onsubmit={handleSubmit} class="flex flex-col gap-1">
			<input type="text" name="username" autocomplete="username" class="hidden" tabindex="-1" />

			<FormToggle
				label={m.pushover_enable({ locale: i18n.languageTag })}
				description={pushoverState.settings.pushover_enabled
					? m.pushover_active({ locale: i18n.languageTag })
					: m.pushover_desc({ locale: i18n.languageTag })}
				checked={pushoverState.settings.pushover_enabled}
				onchange={(e) =>
					pushoverState.updateSetting('pushover_enabled', (e.target as HTMLInputElement).checked)}
			/>

			<PushoverFormFields
				settings={pushoverState.settings}
				updateSetting={pushoverState.updateSetting}
				errors={pushoverState.errors}
			/>
		</form>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.pushover_help_title({ locale })}
		intro={m.pushover_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
