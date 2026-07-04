<script lang="ts">
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import Settings from '~icons/tabler/settings';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormInput, FormSelect } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { WifiHostnameSchema } from '$lib/features/wifi/wifiValidation';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { useZodForm } from '$lib/utils/validation/zodForm.svelte';
	import type { WifiMode } from '$lib/types/connectivity/wifi';
	import { z } from 'zod';

	interface Props {
		hostname: string;
		mode: WifiMode;
		isDirty?: boolean;
		saveBlocked?: boolean;
		onApply?: () => void;
		onReset?: () => void;
		onHostnameChange: (_value: string) => void;
		onModeChange: (_value: WifiMode) => void;
	}

	let {
		hostname,
		mode,
		isDirty = false,
		saveBlocked = false,
		onApply,
		onReset,
		onHostnameChange,
		onModeChange
	}: Props = $props();

	// Single-field schema for just the connection settings part
	const connectionSchema = z.object({
		hostname: WifiHostnameSchema,
		mode: z.enum(['off', 'ap', 'sta'])
	});

	const form = useZodForm({
		schema: connectionSchema,
		// use unwrap to avoid warning about capturing initial values of props
		initialValues: { hostname: '', mode: 'ap' as WifiMode },
		onSubmit: (values) => {
			onHostnameChange(values.hostname);
			onModeChange(values.mode);
		}
	});

	// Initial sync from props (one-time)
	let initialized = $state(false);
	$effect(() => {
		if (!initialized && (hostname || mode !== undefined)) {
			form.handleInput('hostname', hostname, false);
			form.handleInput('mode', mode, false);
			initialized = true;
		}
	});

	// Sync from props if they change after initialization
	$effect(() => {
		if (initialized) {
			if (hostname !== form.values.hostname) {
				form.handleInput('hostname', hostname, false);
			}
			if (mode !== form.values.mode) {
				form.handleInput('mode', mode, false);
			}
		}
	});

	// Sync form values back to parent on change/blur
	function updateParent() {
		onHostnameChange(form.values.hostname);
		onModeChange(form.values.mode);
	}

	const wifiModes: Array<{ id: WifiMode; text: string }> = [
		{ id: 'off', text: m.wifi_mode_off({ locale: i18n.languageTag }) },
		{ id: 'ap', text: m.wifi_mode_ap({ locale: i18n.languageTag }) },
		{ id: 'sta', text: m.wifi_mode_sta({ locale: i18n.languageTag }) }
	];
	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.wifi_sta_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.wifi_sta_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.wifi_sta_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/wifi/ap', label: m.menu_wifi_ap({ locale }) },
		{ href: '/system/status', label: m.menu_status({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<SettingsCard
	title={m.wifi_connection_title({ locale: i18n.languageTag })}
	icon={Settings}
	class="h-full"
	hasChanges={isDirty}
	disabled={saveBlocked}
	onSave={onApply}
	{onReset}
	dirtySourceId="wifi-connection-settings"
>
	{#snippet actions()}
		<HelpTriggerButton
			label={m.wifi_sta_help_title({ locale })}
			onclick={() => (helpOpen = true)}
		/>
	{/snippet}

	<div class="flex flex-col gap-1">
		<ContentBox>
			<FormInput
				id="hostname"
				label={m.wifi_hostname_label({ locale: i18n.languageTag })}
				help={m.wifi_hostname_help({ locale: i18n.languageTag })}
				value={form.values.hostname}
				placeholder="matrixhub"
				oninput={(e) => {
					const val = (e.target as HTMLInputElement).value;
					form.handleInput('hostname', val);
					updateParent();
				}}
				onblur={() => form.handleBlur('hostname')}
				error={form.errors.hostname}
				class="w-full"
				minlength={3}
				maxlength={32}
				required
			/>
		</ContentBox>

		<ContentBox>
			<FormSelect
				id="wifi-mode"
				label={m.wifi_mode_label({ locale: i18n.languageTag })}
				value={form.values.mode}
				onchange={(e) => {
					const val = (e.target as HTMLSelectElement).value as WifiMode;
					form.handleInput('mode', val);
					updateParent();
				}}
				options={wifiModes.map((mode) => ({ value: mode.id, label: mode.text }))}
			/>
		</ContentBox>
	</div>

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.wifi_sta_help_title({ locale })}
		intro={m.wifi_sta_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
