<script lang="ts">
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { useSystemStatusReadModel } from '$lib/features/system/status/useSystemStatusReadModel.svelte';
	import Eye from '~icons/tabler/eye';

	import { Spinner } from '$lib/components';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { calculateCompensationPreview } from './compensationModel';

	let { compState } = $props<{
		compState: ReturnType<
			typeof import('./useCompensationSettings.svelte').useCompensationSettings
		>;
	}>();

	const statusModel = useSystemStatusReadModel();
	const currentCpu = $derived(statusModel.coreTemp ?? 40);
	const preview = $derived(calculateCompensationPreview(compState.settings, currentCpu));

	function handleSave() {
		void compState.saveSettings();
	}
</script>

<SettingsCard
	title={m.comp_preview_title({ locale: i18n.languageTag })}
	icon={Eye}
	class="h-full"
	hasChanges={compState.hasChanges}
	loading={compState.loading}
	saving={compState.saving}
	onSave={handleSave}
	onReset={compState.resetSettings}
	dirtySourceId="compensation-preview"
>
	{#if compState.loading}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else}
		<div class="flex h-full flex-col gap-1">
			<!-- CPU Temperature -->
			<ContentBox>
				<div class="flex justify-between items-center">
					<span class="text-sm font-bold">{m.comp_preview_cpu({ locale: i18n.languageTag })}</span>
					<span class="font-mono text-lg">{currentCpu.toFixed(1)} °C</span>
				</div>
			</ContentBox>

			<!-- Calculated Correction -->
			<ContentBox>
				<div class="flex justify-between items-center">
					<span class="text-sm font-bold"
						>{m.comp_preview_correction({ locale: i18n.languageTag })}</span
					>
					<span class="font-mono font-bold text-primary text-2xl"
						>{preview.correction > 0 ? '-' : '+'}{Math.abs(preview.correction).toFixed(2)} °C</span
					>
				</div>
			</ContentBox>

			<!-- Formula -->
			<ContentBox>
				<div class="text-xs opacity-50 font-mono leading-relaxed">
					{m.comp_preview_formula({ locale: i18n.languageTag })}
					{compState.settings.base_temp_offset.toFixed(1)} + ({currentCpu.toFixed(1)} - {compState.settings.reference_cpu_temp.toFixed(
						1
					)}) × {compState.settings.temp_offset_per_cpu_degree.toFixed(2)}
				</div>
				{#if preview.isClamped}
					<div class="text-xs text-warning mt-1">
						{m.comp_preview_clamped({
							min: compState.settings.min_temp_offset.toFixed(1),
							max: compState.settings.max_temp_offset.toFixed(1)
						})}
					</div>
				{/if}
			</ContentBox>

			<!-- Humidity Impact (approximation: uses CPU temp as rawT stand-in;
			     real compensation uses actual SCD41 rawTemp) -->
			<ContentBox>
				<div class="flex justify-between items-center">
					<span class="text-sm font-bold"
						>{m.comp_preview_humid_factor({ locale: i18n.languageTag })}</span
					>
					<span class="font-mono font-bold text-secondary text-lg">
						{#if preview.humidityDelta === null}
							--
						{:else}
							+{preview.humidityDelta.toFixed(1)}%
						{/if}
					</span>
				</div>
			</ContentBox>

			<!-- Quick Presets -->
			<ContentBox>
				<div class="flex flex-col gap-2">
					<span class="text-xs font-bold opacity-70"
						>{m.comp_preset_label({ locale: i18n.languageTag })}</span
					>
					<div class="grid grid-cols-4 gap-1">
						<button
							type="button"
							class="btn btn-sm btn-outline"
							onclick={() => compState.applyPreset('none')}
							>⊘ {m.comp_preset_none({ locale: i18n.languageTag })}</button
						>
						<button
							type="button"
							class="btn btn-sm btn-outline btn-primary"
							onclick={() => compState.applyPreset('factory')}
							>🏭 {m.comp_preset_factory({ locale: i18n.languageTag })}</button
						>
						<button
							type="button"
							class="btn btn-sm btn-outline btn-info"
							onclick={() => compState.applyPreset('small')}
							>🌡️ {m.comp_preset_small({ locale: i18n.languageTag })}</button
						>
						<button
							type="button"
							class="btn btn-sm btn-outline btn-warning"
							onclick={() => compState.applyPreset('large')}
							>🔥 {m.comp_preset_large({ locale: i18n.languageTag })}</button
						>
					</div>
				</div>
			</ContentBox>

			<!-- Magnus Formula -->
			<ContentBox>
				<div class="flex flex-col gap-2 text-xs opacity-70">
					<div>
						<span class="font-bold">{m.comp_magnus_temp({ locale: i18n.languageTag })}</span>
						<pre class="font-mono mt-1 bg-base-200 rounded p-2">T_comp = T_raw − offset(T_cpu)</pre>
					</div>
					<div>
						<span class="font-bold">{m.comp_magnus_humid({ locale: i18n.languageTag })}</span>
						<pre
							class="font-mono mt-1 bg-base-200 rounded p-2">RH_comp = RH_raw × exp(γ(T_raw) − γ(T_comp))</pre>
					</div>
					<div>
						<span class="font-bold">{m.comp_magnus_gamma({ locale: i18n.languageTag })}</span>
						<pre
							class="font-mono mt-1 bg-base-200 rounded p-2">γ(T) = 17.62 · T / (243.12 + T)</pre>
					</div>
				</div>
			</ContentBox>
		</div>
	{/if}
</SettingsCard>
