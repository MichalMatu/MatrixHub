<script lang="ts">
	import type { NTPSettings } from '$lib/types/connectivity/ntp';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import { validateNtpSettings, formatTimezoneFromLabel, getBrowserTime } from './ntpFormUtils';
	import NtpServerFields from './NtpServerFields.svelte';
	import NtpManualTimeFields from './NtpManualTimeFields.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	// Icons
	import Clock from '~icons/tabler/clock';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		settings: NTPSettings;
		manualTimeInput: string;
		onSubmit: () => void;
		onReset: () => void;
		isDirty: boolean;
	}

	let {
		settings = $bindable(),
		manualTimeInput = $bindable(),
		onSubmit,
		onReset,
		isDirty
	}: Props = $props();

	let formField: HTMLFormElement | undefined = $state();

	let formErrors = $state({
		server: false
	});

	function validateAndSubmit() {
		const validation = validateNtpSettings(settings.enabled, settings.server);
		formErrors.server = validation.errors.server;

		// Update tz_format from tz_label
		settings.tz_format = formatTimezoneFromLabel(settings.tz_label);

		if (validation.valid) {
			onSubmit();
		}
	}

	function useBrowserTime() {
		manualTimeInput = getBrowserTime();
	}

	function handleEnabledChange(enabled: boolean) {
		settings.enabled = enabled;
	}

	function handleServerChange(server: string) {
		settings.server = server;
	}

	function handleTimezoneChange(tzLabel: string) {
		settings.tz_label = tzLabel;
	}

	function preventDefault(fn: () => void | Promise<void>) {
		return function (event: Event) {
			event.preventDefault();
			void fn();
		};
	}

	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.ntp_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.ntp_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.ntp_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/logs', label: m.menu_logs({ locale }) },
		{ href: '/alarms', label: m.menu_alarms({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<SettingsCard
	title={m.ntp_settings_title({ locale: i18n.languageTag })}
	icon={Clock}
	hasChanges={isDirty}
	onSave={validateAndSubmit}
	{onReset}
	dirtySourceId="ntp-settings"
>
	{#snippet actions()}
		<HelpTriggerButton label={m.ntp_help_title({ locale })} onclick={() => (helpOpen = true)} />
	{/snippet}

	<form
		class="flex flex-col gap-1"
		onsubmit={preventDefault(validateAndSubmit)}
		novalidate
		bind:this={formField}
	>
		<ContentBox>
			<fieldset class="flex flex-col gap-1">
				<legend class="label p-0 font-bold mb-1"
					>{m.ntp_label_source({ locale: i18n.languageTag })}</legend
				>
				<div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
					<label
						class="label cursor-pointer justify-start gap-3 border border-base-content/10 rounded-box p-3 hover:bg-base-content/5 transition-colors"
						class:bg-base-200={settings.enabled}
						for="time-source-ntp"
					>
						<input
							id="time-source-ntp"
							type="radio"
							name="time-source"
							class="radio radio-primary"
							checked={settings.enabled}
							onchange={() => handleEnabledChange(true)}
						/>
						<span class="font-medium">{m.ntp_source_ntp({ locale: i18n.languageTag })}</span>
					</label>
					<label
						class="label cursor-pointer justify-start gap-3 border border-base-content/10 rounded-box p-3 hover:bg-base-content/5 transition-colors"
						class:bg-base-200={!settings.enabled}
						for="time-source-manual"
					>
						<input
							id="time-source-manual"
							type="radio"
							name="time-source"
							class="radio radio-primary"
							checked={!settings.enabled}
							onchange={() => handleEnabledChange(false)}
						/>
						<span class="font-medium">{m.ntp_source_manual({ locale: i18n.languageTag })}</span>
					</label>
				</div>
			</fieldset>
		</ContentBox>

		<div class="divider my-4">
			{settings.enabled
				? m.ntp_source_ntp({ locale: i18n.languageTag })
				: m.ntp_source_manual({ locale: i18n.languageTag })}
		</div>

		{#if settings.enabled}
			<NtpServerFields
				server={settings.server}
				tzLabel={settings.tz_label}
				serverError={formErrors.server}
				onServerChange={handleServerChange}
				onTimezoneChange={handleTimezoneChange}
			/>
		{:else}
			<NtpManualTimeFields bind:manualTimeInput onUseBrowserTime={useBrowserTime} />
		{/if}
	</form>

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.ntp_help_title({ locale })}
		intro={m.ntp_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
