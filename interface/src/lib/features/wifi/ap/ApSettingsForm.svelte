<script lang="ts">
	import { Spinner } from '$lib/components';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormInput } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import type { ApSettings } from '$lib/types/connectivity/ap';
	import {
		validateWifiApSettings,
		createWifiApFormErrors,
		type WifiApFormErrors
	} from '$lib/features/wifi/wifiValidation';
	import ApNetworkFields from './ApNetworkFields.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	// Icons
	import AP from '~icons/tabler/access-point';

	interface Props {
		settings: ApSettings | null;
		loading: boolean;
		isDirty: boolean;
		onSave: (_settings: ApSettings) => void;
		onReset: () => void;
	}

	let { settings = $bindable(), loading, isDirty, onSave, onReset }: Props = $props();

	let formField: HTMLFormElement | undefined = $state();
	let formErrors = $state<WifiApFormErrors>(createWifiApFormErrors());

	function validateAndSubmit() {
		if (!settings) return;

		const result = validateWifiApSettings(settings);
		formErrors = result.errors;

		if (result.valid) {
			onSave(settings);
		}
	}

	function preventDefault(fn: () => void) {
		return function (event: Event) {
			event.preventDefault();
			fn();
		};
	}

	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.wifi_ap_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.wifi_ap_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.wifi_ap_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/wifi/sta', label: m.menu_wifi_sta({ locale }) },
		{ href: '/system/status', label: m.menu_status({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<SettingsCard
	title={m.ap_settings_title({ locale: i18n.languageTag })}
	icon={AP}
	hasChanges={isDirty}
	loading={loading || !settings}
	onSave={settings ? validateAndSubmit : undefined}
	{onReset}
	dirtySourceId="wifi-ap-settings"
>
	{#snippet actions()}
		<HelpTriggerButton label={m.wifi_ap_help_title({ locale })} onclick={() => (helpOpen = true)} />
	{/snippet}

	{#if loading || !settings}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<form
			class="fieldset grid w-full grid-cols-1 content-center gap-x-3 gap-y-1 md:grid-cols-2"
			onsubmit={preventDefault(validateAndSubmit)}
			novalidate
			bind:this={formField}
		>
			<ContentBox>
				<FormInput
					label={m.ap_ssid_label({ locale: i18n.languageTag })}
					id="ssid"
					bind:value={settings.ssid}
					minlength={1}
					maxlength={32}
					required
					autocomplete="off"
					error={formErrors.ssid ? m.ap_ssid_error({ locale: i18n.languageTag }) : undefined}
				/>
			</ContentBox>

			<ContentBox>
				<FormInput
					label={m.ap_password_label({ locale: i18n.languageTag })}
					id="pwd"
					type="password"
					bind:value={settings.password}
					maxlength={63}
					autocomplete="current-password"
					error={formErrors.password
						? m.ap_password_error({ locale: i18n.languageTag })
						: undefined}
				/>
			</ContentBox>

			<ContentBox>
				<FormInput
					label={m.ap_channel_label({ locale: i18n.languageTag })}
					id="channel"
					type="number"
					bind:value={settings.channel}
					min={1}
					max={13}
					required
					error={formErrors.channel ? m.ap_channel_error({ locale: i18n.languageTag }) : undefined}
				/>
			</ContentBox>

			<ContentBox>
				<FormInput
					label={m.ap_max_clients_label({ locale: i18n.languageTag })}
					id="clients"
					type="number"
					bind:value={settings.max_clients}
					min={1}
					max={8}
					required
					error={formErrors.max_clients
						? m.ap_max_clients_error({ locale: i18n.languageTag })
						: undefined}
				/>
			</ContentBox>

			<div class="col-span-1 md:col-span-2">
				<ApNetworkFields bind:settings errors={formErrors} />
			</div>
		</form>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.wifi_ap_help_title({ locale })}
		intro={m.wifi_ap_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
