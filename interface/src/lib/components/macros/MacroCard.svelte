<script lang="ts">
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormButton, FormInput, FormSelect, FormToggle } from '$lib/components/shared/forms';
	import { CardHeader } from '$lib/components/common';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import Terminal2 from '~icons/tabler/terminal-2'; // Better macro icon
	import Settings from '~icons/tabler/settings';
	import Plus from '~icons/tabler/plus';
	import Help from '~icons/tabler/help'; // Help Icon
	import InfoCircle from '~icons/tabler/info-circle';
	import * as m from '$lib/paraglide/messages.js';
	import MacroList from './MacroList.svelte';
	import MacroEditor from './MacroEditor.svelte';
	import MacroHelpModal from './MacroHelpModal.svelte'; // Import Help Modal
	import Modal from '$lib/components/Modal.svelte';
	import { fade } from 'svelte/transition';

	import { onMount, onDestroy } from 'svelte';
	import { useMacroManagement } from './useMacroManagement.svelte';

	const controller = useMacroManagement();
	const localSettings = controller.localSettings;
	let showHelp = $state(false); // State for help modal

	onMount(() => {
		void controller.init();
	});

	onDestroy(() => {
		controller.dispose();
	});
</script>

{#snippet headerActions()}
	<FormButton
		variant="icon"
		size="sm"
		icon={Help}
		ariaLabel={m.macros_help_btn()}
		title={m.macros_help_btn()}
		onclick={(e) => {
			e.stopPropagation();
			showHelp = true;
		}}
	/>

	<FormButton
		variant="icon"
		size="sm"
		icon={Plus}
		ariaLabel={m.macros_new_btn()}
		title={m.macros_new_btn()}
		onclick={(e) => {
			e.stopPropagation();
			controller.handleNew();
		}}
	/>
{/snippet}

{#if controller.loading}
	<div class="grid grid-cols-1 md:grid-cols-2 gap-4">
		<div class="h-full">
			<LoadingCard title={m.settings_title()} icon={Settings} loading={true} />
		</div>
		<div class="h-full">
			<LoadingCard title={m.macros_title()} icon={Terminal2} loading={true} />
		</div>
	</div>
{:else}
	{#if controller.error}
		<div class="alert alert-error mb-4" in:fade>
			<span>{controller.error}</span>
		</div>
	{/if}

	<div class="grid grid-cols-1 md:grid-cols-2 gap-4">
		<div class="h-full">
			<SettingsCard
				title={m.settings_title()}
				icon={Settings}
				class="h-full"
				hideTitleOnTiny={false}
				hasChanges={controller.hasSettingsChanges}
				saving={controller.settingsSaving}
				onSave={controller.confirmSaveSettings}
				onReset={controller.resetSettings}
				dirtySourceId="macro-settings"
			>
				<div class="flex flex-col gap-1 h-full" in:fade>
					<FormToggle
						label={m.macros_enabled_aria()}
						description={localSettings.enabled ? m.macro_msg_enabled() : m.macro_msg_disabled()}
						bind:checked={localSettings.enabled}
						disabled={controller.settingsSaving}
					/>

					{#if localSettings.boot_script !== '' && !localSettings.enabled}
						<div class="flex gap-3 items-center p-3 rounded-lg bg-base-200/50 text-info text-sm">
							<InfoCircle class="h-5 w-5 shrink-0" />
							<span>{m.macros_boot_script_hint()}</span>
						</div>
					{/if}

					<ContentBox>
						<FormSelect
							label={m.macros_boot_script_label()}
							bind:value={localSettings.boot_script}
							options={controller.scriptOptions}
							disabled={controller.settingsSaving}
						/>
					</ContentBox>

					<ContentBox>
						<FormInput
							label={m.macros_boot_delay_label()}
							type="number"
							bind:value={localSettings.boot_delay}
							min="0"
							disabled={localSettings.boot_script === '' || controller.settingsSaving}
						/>
					</ContentBox>
				</div>
			</SettingsCard>
		</div>

		<div class="h-full">
			<BaseCard class="h-full">
				<!-- Title is "Macros" -->
				<CardHeader title={m.macros_title()} icon={Terminal2} actions={headerActions} />

				<div in:fade>
					{#if !controller.settings.enabled}
						<div class="py-2 mb-2 text-center opacity-40 text-sm">
							{m.macros_disabled_msg()}<br />
							<span class="text-xs">{m.macros_disabled_hint()}</span>
						</div>
					{/if}

					<MacroList
						api={controller.apiService}
						onrun={(name) => controller.runScript(name)}
						onstop={() => controller.stopScript()}
						ondelete={(name) => controller.deleteScript(name)}
						onedit={(name) => controller.handleEdit(name)}
						scripts={controller.scriptList}
						loading={controller.loading}
						enabled={controller.settings.enabled}
					/>
				</div>
			</BaseCard>
		</div>
	</div>
{/if}

<!-- Editor Modal -->
<Modal
	isOpen={controller.showEditor}
	onClose={() => (controller.showEditor = false)}
	title={controller.isNew
		? m.macros_modal_new()
		: m.macros_modal_edit({ filename: controller.editFilename })}
	widthClass="max-w-2xl"
>
	<MacroEditor
		bind:filename={controller.editFilename}
		bind:content={controller.editContent}
		isNew={controller.isNew}
	/>

	{#snippet actions()}
		<div class="flex justify-end gap-2 w-full">
			<FormButton
				variant="ghost"
				label={m.macros_editor_cancel()}
				onclick={() => (controller.showEditor = false)}
			/>
			<FormButton
				variant="primary"
				label={m.macros_editor_save()}
				onclick={() =>
					controller.saveScript({
						filename: controller.editFilename,
						content: controller.editContent
					})}
				disabled={controller.isSavingScript}
				loading={controller.isSavingScript}
			/>
		</div>
	{/snippet}
</Modal>

<!-- Settings Modal -->
<!-- Help Modal -->
<MacroHelpModal bind:isOpen={showHelp} />
