<script lang="ts">
	import { Spinner } from '$lib/components';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormToggle, FormButton } from '$lib/components/shared/forms';
	import Settings from '~icons/tabler/settings';
	import Save from '~icons/tabler/device-floppy';
	import type { BleSettings } from '$lib/types/connectivity/ble';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		savedSettings: BleSettings | null;
		localEnabled: boolean;
		hasChanges: boolean;
		saving: boolean;
		onSave: () => void;
	}

	let { savedSettings, localEnabled = $bindable(), hasChanges, saving, onSave }: Props = $props();

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

<BaseCard title={m.ble_settings_title({ locale: i18n.languageTag })} icon={Settings}>
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

			<div class="flex justify-end mt-1">
				<FormButton
					label={m.action_save({ locale: i18n.languageTag })}
					icon={Save}
					disabled={!hasChanges}
					loading={saving}
					onclick={onSave}
				/>
			</div>
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
</BaseCard>
