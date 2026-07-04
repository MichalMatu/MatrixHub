<script lang="ts">
	import Plus from '~icons/tabler/plus';
	import Bolt from '~icons/tabler/bolt';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import ShellyDeviceRow from '$lib/features/shelly/ShellyDeviceRow.svelte';
	import ShellyAddDeviceModal from '$lib/features/shelly/ShellyAddDeviceModal.svelte';
	import { useShellyDevices } from '$lib/features/shelly/useShellyDevices.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { PageWrapper } from '$lib/components/layout';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { Spinner } from '$lib/components';
	import { createFeatureLinks, shellyHelpLinkIds } from '$lib/features/navigation/featureRegistry';

	let { data } = $props();
	const shellyDevices = useShellyDevices({
		initialDevicesFn: () => data.devices || []
	});
	const canManage = $derived(shellyDevices.canManage);
	const shellyState = $derived(shellyDevices.state);
	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.shelly_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.shelly_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.shelly_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived(createFeatureLinks(shellyHelpLinkIds, locale));
</script>

<PageWrapper>
	<BaseCard
		title={m.shelly_page_title({ locale: i18n.languageTag })}
		icon={Bolt}
		iconClass="h-6 w-6 text-primary flex-none"
	>
		{#snippet actions()}
			<div class="flex items-center gap-2">
				<HelpTriggerButton
					label={m.shelly_help_title({ locale })}
					onclick={() => (helpOpen = true)}
				/>
				{#if canManage}
					<FormButton
						label={m.shelly_btn_add({ locale: i18n.languageTag })}
						icon={Plus}
						onclick={shellyDevices.openAddModal}
					/>
				{/if}
			</div>
		{/snippet}

		{#if data.loadError}
			<div class="alert alert-error mb-3">
				<span>{m.error_prefix({ error: data.loadError }, { locale: i18n.languageTag })}</span>
			</div>
		{/if}
		{#if shellyState.error}
			<div class="alert alert-warning mb-3">
				<span>{m.error_prefix({ error: shellyState.error }, { locale: i18n.languageTag })}</span>
			</div>
		{/if}
		<div class="flex flex-col gap-1">
			{#if shellyState.loading}
				<ContentBox paddingClass="py-8" class="flex items-center justify-center">
					<Spinner />
				</ContentBox>
			{:else}
				{#each shellyState.devices as dev (dev.id)}
					<ShellyDeviceRow
						device={dev}
						{canManage}
						onToggle={(newState) => shellyDevices.toggleDevice(dev.id, newState)}
						onEdit={() => shellyDevices.openEditModal(dev)}
						onDelete={() => shellyDevices.removeDevice(dev.id)}
					/>
				{:else}
					<ContentBox paddingClass="py-8" class="text-center text-base-content/50">
						{m.shelly_empty_list({ locale: i18n.languageTag })}
					</ContentBox>
				{/each}
			{/if}
		</div>
	</BaseCard>

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.shelly_help_title({ locale })}
		intro={m.shelly_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</PageWrapper>

{#if canManage}
	<ShellyAddDeviceModal
		isOpen={shellyState.adding}
		editing={shellyState.editingDeviceId !== null}
		saving={shellyState.saving}
		deviceName={shellyState.newDevice.name}
		deviceIp={shellyState.newDevice.ip}
		deviceRelay={shellyState.newDevice.relay_index}
		deviceGeneration={shellyState.newDevice.generation}
		deviceCount={shellyState.devices.length}
		errorMessage={shellyState.error}
		onClose={shellyDevices.closeAddModal}
		onSave={shellyDevices.saveDevice}
		onNameChange={shellyDevices.updateNewDeviceName}
		onIpChange={shellyDevices.updateNewDeviceIp}
		onRelayChange={shellyDevices.updateNewDeviceRelay}
		onGenerationChange={shellyDevices.updateNewDeviceGeneration}
	/>
{/if}
