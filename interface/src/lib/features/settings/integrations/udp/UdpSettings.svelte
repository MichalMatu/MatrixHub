<script lang="ts">
	import Network from '~icons/tabler/network';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { Spinner } from '$lib/components';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormToggle, FormInput, FormSelect } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	let { udpState } = $props<{
		udpState: ReturnType<typeof import('./useUdpSettings.svelte').useUdpSettings>;
	}>();

	function handleSubmit(event: Event) {
		event.preventDefault();
		udpState.saveSettings();
	}

	function handleSave() {
		udpState.saveSettings();
	}

	const formatOptions = [
		{ value: 'line', label: m.udp_fmt_influx({ locale: i18n.languageTag }) },
		{ value: 'json', label: m.udp_fmt_json({ locale: i18n.languageTag }) },
		{ value: 'csv', label: m.udp_fmt_csv({ locale: i18n.languageTag }) }
	];

	const intervalOptions = [
		{ value: 10000, label: m.udp_int_10s({ locale: i18n.languageTag }) },
		{ value: 30000, label: m.udp_int_30s({ locale: i18n.languageTag }) },
		{
			value: 60000,
			label: m.udp_int_1m(
				{ unit: m.unit_min({ locale: i18n.languageTag }) },
				{ locale: i18n.languageTag }
			)
		},
		{
			value: 300000,
			label: m.udp_int_5m(
				{ unit: m.unit_min({ locale: i18n.languageTag }) },
				{ locale: i18n.languageTag }
			)
		},
		{
			value: 600000,
			label: m.udp_int_10m(
				{ unit: m.unit_min({ locale: i18n.languageTag }) },
				{ locale: i18n.languageTag }
			)
		}
	];
	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.udp_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.udp_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.udp_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/logs', label: m.menu_logs({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<SettingsCard
	title={m.udp_title({ locale: i18n.languageTag })}
	icon={Network}
	hasChanges={udpState.hasChanges}
	loading={udpState.loading}
	saving={udpState.saving}
	onSave={handleSave}
	onReset={udpState.resetSettings}
	dirtySourceId="udp-settings"
>
	{#snippet actions()}
		<HelpTriggerButton label={m.udp_help_title({ locale })} onclick={() => (helpOpen = true)} />
	{/snippet}

	{#if udpState.loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<form onsubmit={handleSubmit} class="flex flex-col gap-1" novalidate>
			<FormToggle
				label={m.udp_enable({ locale: i18n.languageTag })}
				description={udpState.settings.enabled
					? `→ ${udpState.settings.host || '?'}:${udpState.settings.port}`
					: m.udp_disabled({ locale: i18n.languageTag })}
				bind:checked={udpState.settings.enabled}
			/>

			<ContentBox>
				<div class="grid grid-cols-1 sm:grid-cols-3 gap-2">
					<div class="sm:col-span-2">
						<FormInput
							label={m.udp_host({ locale: i18n.languageTag })}
							id="udp_host"
							value={udpState.settings.host}
							oninput={(e) => {
								const val = (e.target as HTMLInputElement).value;
								udpState.updateSetting('host', val);
							}}
							placeholder={m.udp_host_placeholder({ locale: i18n.languageTag })}
							error={udpState.errors.host
								? m.udp_host_error({ locale: i18n.languageTag })
								: undefined}
							maxlength={63}
						/>
					</div>
					<div>
						<FormInput
							label={m.udp_port({ locale: i18n.languageTag })}
							id="udp_port"
							type="number"
							value={udpState.settings.port}
							oninput={(e) =>
								udpState.updateSetting('port', Number((e.target as HTMLInputElement).value))}
							min={1}
							max={65535}
							error={udpState.errors.port
								? m.udp_port_error({ locale: i18n.languageTag })
								: undefined}
						/>
					</div>
				</div>
			</ContentBox>

			<ContentBox>
				<FormSelect
					label={m.udp_format({ locale: i18n.languageTag })}
					id="udp_format"
					options={formatOptions}
					value={udpState.settings.format}
					onchange={(e) =>
						udpState.setFormat((e.target as HTMLSelectElement).value as 'line' | 'json' | 'csv')}
				/>
			</ContentBox>

			<ContentBox>
				<FormSelect
					label={m.udp_interval({ locale: i18n.languageTag })}
					id="udp_interval"
					options={intervalOptions}
					value={udpState.settings.interval_ms}
					onchange={(e) => udpState.setInterval(Number((e.target as HTMLSelectElement).value))}
				/>
			</ContentBox>
		</form>
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.udp_help_title({ locale })}
		intro={m.udp_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</SettingsCard>
