<script lang="ts">
	import Temperature from '~icons/tabler/temperature';
	import Save from '~icons/tabler/device-floppy';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { useSystemStatusReadModel } from '$lib/features/system/status/useSystemStatusReadModel.svelte';
	import { Spinner } from '$lib/components';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormRange, FormButton, FormToggle } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	let { compState, showSaveButton = true } = $props<{
		compState: ReturnType<
			typeof import('./useCompensationSettings.svelte').useCompensationSettings
		>;
		showSaveButton?: boolean;
	}>();

	const statusModel = useSystemStatusReadModel();
	const currentCpuLabel = $derived(statusModel.coreTemp?.toFixed(1) ?? '--');

	function handleSubmit(event: Event) {
		event.preventDefault();
		void compState.saveSettings();
	}
</script>

<BaseCard title={m.comp_title({ locale: i18n.languageTag })} icon={Temperature} class="h-full">
	{#if compState.loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<form onsubmit={handleSubmit} class="flex h-full flex-col gap-1" novalidate>
			<FormToggle
				label={m.comp_enable({ locale: i18n.languageTag })}
				description={compState.settings.enabled
					? m.comp_enabled_desc({ locale: i18n.languageTag })
					: m.comp_disabled_desc({ locale: i18n.languageTag })}
				checked={compState.settings.enabled}
				onchange={(e) => compState.updateSetting('enabled', (e.target as HTMLInputElement).checked)}
			/>

			<ContentBox>
				<FormRange
					label={m.comp_base_offset({ locale: i18n.languageTag })}
					description={m.comp_base_offset_desc({ locale: i18n.languageTag })}
					id="comp_base_offset"
					bind:value={compState.settings.base_temp_offset}
					min={-5}
					max={20}
					step={0.1}
					suffix="°C"
				/>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={`${m.comp_ref_cpu({ locale: i18n.languageTag })} (${currentCpuLabel} °C)`}
					description={m.comp_ref_cpu_desc({ locale: i18n.languageTag })}
					id="comp_ref_cpu"
					bind:value={compState.settings.reference_cpu_temp}
					min={20}
					max={80}
					step={0.5}
					suffix="°C"
				/>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={`${m.comp_slope({ locale: i18n.languageTag })} (CPU: ${currentCpuLabel} °C)`}
					description={m.comp_slope_desc({ locale: i18n.languageTag })}
					id="comp_slope"
					bind:value={compState.settings.temp_offset_per_cpu_degree}
					min={0}
					max={2}
					step={0.05}
					suffix="°C"
				/>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={m.comp_min_offset({ locale: i18n.languageTag })}
					description={m.comp_min_offset_desc({ locale: i18n.languageTag })}
					id="comp_min_offset"
					bind:value={compState.settings.min_temp_offset}
					min={-10}
					max={25}
					step={0.1}
					suffix="°C"
				/>
			</ContentBox>

			<ContentBox>
				<FormRange
					label={m.comp_max_offset({ locale: i18n.languageTag })}
					description={m.comp_max_offset_desc({ locale: i18n.languageTag })}
					id="comp_max_offset"
					bind:value={compState.settings.max_temp_offset}
					min={-10}
					max={25}
					step={0.1}
					suffix="°C"
				/>
			</ContentBox>

			{#if showSaveButton}
				<div class="mt-auto flex justify-end pt-4">
					<FormButton
						type="submit"
						label={m.action_save({ locale: i18n.languageTag })}
						icon={Save}
						loading={compState.saving}
						disabled={!compState.hasChanges || compState.saving}
					/>
				</div>
			{/if}
		</form>
	{/if}
</BaseCard>
