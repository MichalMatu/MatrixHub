<script lang="ts">
	import { Spinner } from '$lib/components';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormToggle, FormRange } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import Settings from '~icons/tabler/settings';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import {
		createFeatureLinks,
		wifiSensingHelpLinkIds
	} from '$lib/features/navigation/featureRegistry';

	import type { WifiSensingSettings } from '$lib/types/connectivity/wifiSensing';

	interface Props {
		savedSettings: WifiSensingSettings | null;
		localEnabled: boolean;
		localInterval: number;
		localThreshold: number;
		hasChanges: boolean;
		saving: boolean;
		onSave: () => void;
		onReset: () => void;
	}

	let {
		savedSettings,
		localEnabled = $bindable(),
		localInterval = $bindable(),
		localThreshold = $bindable(),
		hasChanges,
		saving,
		onSave,
		onReset
	}: Props = $props();

	function handleSubmit(event: Event) {
		event.preventDefault();
		onSave();
	}

	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.sensing_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.sensing_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.sensing_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived(createFeatureLinks(wifiSensingHelpLinkIds, locale));
</script>

<SettingsCard
	title={m.sensing_settings_title({ locale: i18n.languageTag })}
	icon={Settings}
	class="h-full"
	{hasChanges}
	{saving}
	onSave={savedSettings ? onSave : undefined}
	{onReset}
	dirtySourceId="wifi-sensing-settings"
>
	{#snippet actions()}
		<HelpTriggerButton label={m.sensing_help_title({ locale })} onclick={() => (helpOpen = true)} />
	{/snippet}

	{#if savedSettings}
		<form class="flex w-full flex-col gap-1" onsubmit={handleSubmit}>
			<!-- Enable Toggle -->
			<FormToggle
				label={m.sensing_toggle_label({ locale: i18n.languageTag })}
				description={localEnabled
					? m.sensing_toggle_desc_on({ interval: localInterval }, { locale: i18n.languageTag })
					: m.sensing_toggle_desc_off({ locale: i18n.languageTag })}
				bind:checked={localEnabled}
			/>

			<!-- Sample Interval -->
			<ContentBox>
				<FormRange
					label={m.sensing_interval_label({ locale: i18n.languageTag })}
					bind:value={localInterval}
					min={500}
					max={5000}
					step={500}
					suffix="ms"
				/>
			</ContentBox>

			<!-- Variance Threshold -->
			<ContentBox>
				<FormRange
					label={m.sensing_threshold_label({ locale: i18n.languageTag })}
					bind:value={localThreshold}
					min={1}
					max={30}
					step={1}
				/>
			</ContentBox>
		</form>
	{:else}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.sensing_help_title({ locale })}
		intro={m.sensing_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
