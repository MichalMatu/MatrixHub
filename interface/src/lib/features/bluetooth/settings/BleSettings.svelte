<script lang="ts">
	import { Spinner } from '$lib/components';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormToggle } from '$lib/components/shared/forms';
	import Settings from '~icons/tabler/settings';
	import type { BleSettings } from '$lib/types/connectivity/ble';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		savedSettings: BleSettings | null;
		localEnabled: boolean;
		hasChanges: boolean;
		saving: boolean;
		onSave: () => void;
		onReset: () => void;
	}

	let {
		savedSettings,
		localEnabled = $bindable(),
		hasChanges,
		saving,
		onSave,
		onReset
	}: Props = $props();

	let scannerDesc = $derived(
		localEnabled
			? m.ble_scanner_on({ locale: i18n.languageTag })
			: m.ble_scanner_off({ locale: i18n.languageTag })
	);
	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.ble_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.ble_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.ble_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/alarms', label: m.menu_alarms({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<SettingsCard
	title={m.ble_settings_title({ locale: i18n.languageTag })}
	icon={Settings}
	{hasChanges}
	{saving}
	onSave={savedSettings ? onSave : undefined}
	{onReset}
	dirtySourceId="ble-settings"
>
	{#snippet actions()}
		<HelpTriggerButton label={m.ble_help_title({ locale })} onclick={() => (helpOpen = true)} />
	{/snippet}

	{#if savedSettings}
		<div class="flex flex-col gap-1">
			<FormToggle
				label={m.ble_scanner_label({ locale: i18n.languageTag })}
				description={scannerDesc}
				bind:checked={localEnabled}
			/>
		</div>
	{:else}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.ble_help_title({ locale })}
		intro={m.ble_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
