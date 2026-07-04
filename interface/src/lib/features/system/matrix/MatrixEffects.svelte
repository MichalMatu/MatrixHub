<script lang="ts">
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormColorInput, FormRange, FormSelect, FormToggle } from '$lib/components/shared/forms';
	import { Spinner } from '$lib/components/common';
	import IconWand from '~icons/tabler/wand';
	import { type useMatrixSettings } from './useMatrixSettings.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { getMatrixEffectName } from './matrixEffectLabels';
	import {
		MATRIX_COLOR_PRESETS,
		MATRIX_EFFECT_ENGINE_LEGACY,
		MATRIX_EFFECT_ENGINE_NATIVE_3D,
		MATRIX_REACTIVITY_PROVIDER_IMU,
		MATRIX_REACTIVITY_PROVIDER_NONE,
		MATRIX_BACKGROUND_MODE_EFFECTS,
		MATRIX_EFFECT_REACTIVITY_GAIN_MAX,
		MATRIX_EFFECT_REACTIVITY_GAIN_MIN,
		type MatrixEffectSpeedScale,
		type MatrixEffectCategoryId,
		type MatrixEffectEngine,
		type MatrixEffectReactivityProvider,
		type MatrixColorPresetDefinition,
		fromMatrixHexColor,
		MATRIX_EFFECT_SPEED_SCALE_CONFIG,
		MATRIX_EFFECT_SPEED_SCALES,
		fromMatrixEffectSpeedScaleValue,
		getMatrixEffectCategories,
		getMatrixEffectCategory,
		getMatrixEffectIds,
		getPreferredMatrixEffectSpeedScale,
		getPreferredMatrixEffectCategory,
		matrixEffectCategoryContainsEffect,
		normalizeMatrixEffectEngine,
		normalizeMatrixEffectModeForEngine,
		normalizeMatrixEffectSpeedForScale,
		toMatrixHexColor
	} from './matrixModel';

	type MatrixEffectCategory = {
		value: MatrixEffectCategoryId;
		label: string;
		effectIds: number[];
	};

	type MatrixColorPreset = MatrixColorPresetDefinition & {
		label: string;
	};

	let {
		store,
		canManage = true
	}: {
		store: ReturnType<typeof useMatrixSettings>;
		canManage?: boolean;
	} = $props();

	// Derived hex value for local input binding
	let hexColor = $state('#00FF00');
	let hexColor2 = $state('#000000');
	let hexColor3 = $state('#0000FF');
	let speedScale = $state<MatrixEffectSpeedScale>('ms');
	let speedSliderValue = $state(1000);
	let speedScaleInitialized = $state(false);
	let effectCategory = $state<MatrixEffectCategoryId>('recommended');
	let effectCategoryInitialized = $state(false);
	let lastEffectEngine = $state<MatrixEffectEngine>(MATRIX_EFFECT_ENGINE_LEGACY);

	const effectCategories = $derived.by<MatrixEffectCategory[]>(() =>
		getMatrixEffectCategories(normalizeMatrixEffectEngine(store.settings.effect_engine)).map((category) => ({
			...category,
			label: getEffectCategoryLabel(category.value)
		}))
	);

	const colorPresets = $derived.by<MatrixColorPreset[]>(() =>
		MATRIX_COLOR_PRESETS.map((preset) => ({
			...preset,
			label: getColorPresetLabel(preset.id)
		}))
	);

	function getCategoryById(categoryId: MatrixEffectCategoryId): MatrixEffectCategory | undefined {
		return effectCategories.find((category) => category.value === categoryId);
	}

	function getEffectCategoryLabel(categoryId: MatrixEffectCategoryId): string {
		const labels: Record<MatrixEffectCategoryId, string> = {
			recommended: m.matrix_effect_category_recommended({ locale: i18n.languageTag }),
			calm: m.matrix_effect_category_calm({ locale: i18n.languageTag }),
			dynamic: m.matrix_effect_category_dynamic({ locale: i18n.languageTag }),
			seasonal: m.matrix_effect_category_seasonal({ locale: i18n.languageTag }),
			all: m.matrix_effect_category_all({ locale: i18n.languageTag })
		};
		return labels[categoryId];
	}

	function getColorPresetLabel(presetId: string): string {
		const labels: Record<string, string> = {
			alert: m.matrix_effect_palette_alert({ locale: i18n.languageTag }),
			forest: m.matrix_effect_palette_forest({ locale: i18n.languageTag }),
			ocean: m.matrix_effect_palette_ocean({ locale: i18n.languageTag }),
			sunset: m.matrix_effect_palette_sunset({ locale: i18n.languageTag }),
			neon: m.matrix_effect_palette_neon({ locale: i18n.languageTag }),
			aurora: m.matrix_effect_palette_aurora({ locale: i18n.languageTag })
		};
		return labels[presetId] ?? presetId;
	}

	// Watch for external updates to sync input
	$effect(() => {
		if (!store.loading) {
			hexColor = toMatrixHexColor(store.settings.effect_color);
			hexColor2 = toMatrixHexColor(store.settings.effect_color_2);
			hexColor3 = toMatrixHexColor(store.settings.effect_color_3);

			if (!speedScaleInitialized) {
				speedScale = getPreferredMatrixEffectSpeedScale(store.settings.effect_speed);
				speedScaleInitialized = true;
			}
			speedSliderValue =
				normalizeMatrixEffectSpeedForScale(store.settings.effect_speed, speedScale) /
				MATRIX_EFFECT_SPEED_SCALE_CONFIG[speedScale].unitMs;

			if (!effectCategoryInitialized) {
				effectCategory = getPreferredMatrixEffectCategory(
					store.settings.effect_mode,
					normalizeMatrixEffectEngine(store.settings.effect_engine)
				);
				effectCategoryInitialized = true;
			}

			const engine = normalizeMatrixEffectEngine(store.settings.effect_engine);
			if (lastEffectEngine !== engine) {
				lastEffectEngine = engine;
				effectCategory = getPreferredMatrixEffectCategory(store.settings.effect_mode, engine);
			}
		}
	});

	$effect(() => {
		if (store.loading) {
			speedScaleInitialized = false;
			effectCategoryInitialized = false;
		}
	});

	// Handle color change
	function handleColorChange(e: Event) {
		if (!canManage) return;
		const target = e.target as HTMLInputElement;
		hexColor = target.value;
		store.settings.effect_color = fromMatrixHexColor(hexColor);
	}

	function handleColorChange2(e: Event) {
		if (!canManage) return;
		const target = e.target as HTMLInputElement;
		hexColor2 = target.value;
		store.settings.effect_color_2 = fromMatrixHexColor(hexColor2);
	}

	function handleColorChange3(e: Event) {
		if (!canManage) return;
		const target = e.target as HTMLInputElement;
		hexColor3 = target.value;
		store.settings.effect_color_3 = fromMatrixHexColor(hexColor3);
	}

	function handleEffectSpeedChange(e: Event) {
		if (!canManage) return;
		const target = e.target as HTMLInputElement;
		speedSliderValue = Number(target.value);
		store.settings.effect_speed = fromMatrixEffectSpeedScaleValue(speedSliderValue, speedScale);
	}

	function selectSpeedScale(nextScale: MatrixEffectSpeedScale) {
		if (speedScale === nextScale) return;
		speedScale = nextScale;
		speedScaleInitialized = true;

		const normalizedSpeed = normalizeMatrixEffectSpeedForScale(
			store.settings.effect_speed,
			nextScale
		);
		speedSliderValue = normalizedSpeed / MATRIX_EFFECT_SPEED_SCALE_CONFIG[nextScale].unitMs;

		if (canManage) {
			store.settings.effect_speed = normalizedSpeed;
		}
	}

	function applySelectedEffect(effectId: number) {
		const engine = normalizeMatrixEffectEngine(store.settings.effect_engine);
		const normalizedEffectId = normalizeMatrixEffectModeForEngine(effectId, engine);
		if (!canManage || store.settings.effect_mode === normalizedEffectId) {
			return;
		}

		store.updateSetting('effect_mode', normalizedEffectId);
	}

	function selectEffectEngine(nextEngine: MatrixEffectEngine) {
		if (!canManage || store.settings.effect_engine === nextEngine) {
			return;
		}

		store.settings.effect_engine = nextEngine;
		store.settings.effect_mode = normalizeMatrixEffectModeForEngine(
			store.settings.effect_mode,
			nextEngine
		);
		effectCategory = getPreferredMatrixEffectCategory(store.settings.effect_mode, nextEngine);
		effectCategoryInitialized = true;
	}

	function handleEffectCategoryChange(e: Event) {
		const nextCategory = (e.currentTarget as HTMLSelectElement).value as MatrixEffectCategoryId;
		const engine = normalizeMatrixEffectEngine(store.settings.effect_engine);
		effectCategory = nextCategory;
		effectCategoryInitialized = true;

		if (matrixEffectCategoryContainsEffect(nextCategory, store.settings.effect_mode, engine)) {
			return;
		}

		const nextEffect = getMatrixEffectCategory(nextCategory, engine)?.effectIds[0];
		if (nextEffect !== undefined && canManage) {
			applySelectedEffect(nextEffect);
		}
	}

	function handleEffectEnabledChange(e: Event) {
		if (!canManage) return;
		const enabled = (e.currentTarget as HTMLInputElement).checked;
		store.settings.effect_enabled = enabled;
		if (enabled) {
			store.settings.background_mode = MATRIX_BACKGROUND_MODE_EFFECTS;
			store.settings.data_visualization_enabled = false;
		}
	}

	function selectReactivityProvider(nextProvider: MatrixEffectReactivityProvider) {
		if (!canManage) return;
		store.settings.effect_reactivity_provider = nextProvider;
	}

	function applyColorPreset(preset: MatrixColorPreset) {
		if (!canManage) return;

		store.settings.effect_color = preset.colors[0];
		store.settings.effect_color_2 = preset.colors[1];
		store.settings.effect_color_3 = preset.colors[2];

		hexColor = toMatrixHexColor(preset.colors[0]);
		hexColor2 = toMatrixHexColor(preset.colors[1]);
		hexColor3 = toMatrixHexColor(preset.colors[2]);
	}

	const effectCategoryOptions = $derived.by(() =>
		effectCategories.map((category) => ({
			value: category.value,
			label: category.label
		}))
	);

	const effectOptions = $derived.by(() =>
		(
			getCategoryById(effectCategory)?.effectIds ??
			getMatrixEffectIds(normalizeMatrixEffectEngine(store.settings.effect_engine))
		).map((effectId) => ({
			value: effectId,
			label: getMatrixEffectName(
				effectId,
				i18n.languageTag,
				normalizeMatrixEffectEngine(store.settings.effect_engine)
			)
		}))
	);

	const effectEngineOptions = $derived.by(() => [
		{
			value: MATRIX_EFFECT_ENGINE_LEGACY,
			label: m.matrix_effect_engine_classic({ locale: i18n.languageTag })
		},
		{
			value: MATRIX_EFFECT_ENGINE_NATIVE_3D,
			label: m.matrix_effect_engine_native_3d({ locale: i18n.languageTag })
		}
	]);

	const reactivityProviderOptions = $derived.by(() => [
		{
			value: MATRIX_REACTIVITY_PROVIDER_NONE,
			label: m.matrix_effect_reactivity_provider_none({ locale: i18n.languageTag })
		},
		{
			value: MATRIX_REACTIVITY_PROVIDER_IMU,
			label: m.matrix_effect_reactivity_provider_imu({ locale: i18n.languageTag })
		}
	]);

	const speedScaleConfig = $derived.by(() => MATRIX_EFFECT_SPEED_SCALE_CONFIG[speedScale]);
	const effectControlsDisabled = $derived(!canManage || !store.settings.effect_enabled);
	const normalizedEffectEngine = $derived(
		normalizeMatrixEffectEngine(store.settings.effect_engine)
	);
	const native3DControlsVisible = $derived(
		normalizedEffectEngine === MATRIX_EFFECT_ENGINE_NATIVE_3D
	);
	const reactivityControlsDisabled = $derived(
		effectControlsDisabled ||
			!native3DControlsVisible ||
			store.settings.effect_reactivity_provider === MATRIX_REACTIVITY_PROVIDER_NONE
	);
	const activeColorPresetId = $derived.by(() => {
		const activePreset = colorPresets.find(
			(preset) =>
				preset.colors[0] === store.settings.effect_color &&
				preset.colors[1] === store.settings.effect_color_2 &&
				preset.colors[2] === store.settings.effect_color_3
		);

		return activePreset?.id;
	});
</script>

<SettingsCard
	title={m.matrix_effects_title()}
	icon={IconWand}
	hasChanges={store.hasChanges}
	loading={store.loading}
	saving={store.saving}
	disabled={!canManage}
	error={store.error}
	onSave={store.saveSettingsNow}
	onReset={store.resetSettings}
	dirtySourceId="matrix-effects"
>
	<div class="flex w-full flex-col gap-1">
		{#if store.loading}
			<div class="flex justify-center items-center py-8">
				<Spinner />
			</div>
		{:else if store.error}
			<div class="text-error text-sm p-2 bg-error/10 rounded">
				{store.error}
			</div>
		{:else}
			<ContentBox class="flex items-center justify-between">
				<div>
					<div class="flex flex-wrap items-center gap-2">
						<span class="font-bold text-sm">{m.matrix_effects_enable()}</span>
						<span
							class="badge badge-sm {store.settings.effect_enabled
								? 'badge-success'
								: 'badge-ghost'}"
						>
							{store.settings.effect_enabled
								? m.imu_state_enabled({ locale: i18n.languageTag })
								: m.imu_state_disabled({ locale: i18n.languageTag })}
						</span>
					</div>
					<p class="text-xs text-base-content/70">{m.matrix_effects_desc()}</p>
					{#if !store.settings.effect_enabled}
						<p class="mt-1 text-xs text-base-content/60">
							{m.matrix_effects_disabled_hint()}
						</p>
					{/if}
				</div>
				<FormToggle
					label=""
					bind:checked={store.settings.effect_enabled}
					disabled={!canManage}
					ariaLabel={m.matrix_effects_enable({ locale: i18n.languageTag })}
					plain={true}
					onchange={handleEffectEnabledChange}
				/>
			</ContentBox>

			<div class="flex flex-col gap-1" aria-disabled={!store.settings.effect_enabled}>
				<ContentBox>
					<FormSelect
						label={m.matrix_effect_engine()}
						value={store.settings.effect_engine}
						options={effectEngineOptions}
						disabled={effectControlsDisabled}
						onchange={(e) =>
							selectEffectEngine(
								Number((e.target as HTMLSelectElement).value) as MatrixEffectEngine
							)}
					/>
				</ContentBox>

				<ContentBox>
					<FormSelect
						label={m.matrix_effect_category()}
						value={effectCategory}
						options={effectCategoryOptions}
						disabled={effectControlsDisabled}
						onchange={handleEffectCategoryChange}
					/>
				</ContentBox>

				<ContentBox>
					<FormSelect
						label={m.matrix_effect_mode()}
						value={store.settings.effect_mode}
						options={effectOptions}
						help={m.matrix_effect_mode_live_desc({ locale: i18n.languageTag })}
						disabled={effectControlsDisabled}
						onchange={(e) =>
							void applySelectedEffect(Number((e.target as HTMLSelectElement).value))}
					/>
				</ContentBox>

				<ContentBox>
					<FormRange
						label={m.matrix_effect_speed()}
						min={speedScaleConfig.min}
						max={speedScaleConfig.max}
						step={speedScaleConfig.step}
						suffix={speedScaleConfig.suffix}
						valueClass="w-20"
						disabled={effectControlsDisabled}
						bind:value={speedSliderValue}
						oninput={handleEffectSpeedChange}
					/>
					<div class="mt-4 grid grid-cols-2 gap-3 sm:grid-cols-4">
						{#each MATRIX_EFFECT_SPEED_SCALES as scale}
							<button
								type="button"
								class="btn btn-sm h-10 w-full font-semibold uppercase tracking-wide {speedScale ===
								scale
									? 'btn-primary'
									: 'btn-outline btn-primary'}"
								disabled={effectControlsDisabled}
								aria-pressed={speedScale === scale}
								onclick={() => selectSpeedScale(scale)}
							>
								{scale}
							</button>
						{/each}
					</div>
				</ContentBox>

				{#if native3DControlsVisible}
					<ContentBox>
						<FormSelect
							label={m.matrix_effect_reactivity_provider()}
							value={store.settings.effect_reactivity_provider}
							options={reactivityProviderOptions}
							disabled={effectControlsDisabled}
							onchange={(e) =>
								selectReactivityProvider(
									Number(
										(e.target as HTMLSelectElement).value
									) as MatrixEffectReactivityProvider
								)}
						/>
					</ContentBox>

					<ContentBox>
						<FormRange
							label={m.matrix_effect_reactivity_gain()}
							min={MATRIX_EFFECT_REACTIVITY_GAIN_MIN}
							max={MATRIX_EFFECT_REACTIVITY_GAIN_MAX}
							step={5}
							suffix="%"
							valueClass="w-20"
							disabled={reactivityControlsDisabled}
							bind:value={store.settings.effect_reactivity_gain}
						/>
					</ContentBox>
				{/if}

				<ContentBox class="flex flex-col gap-3">
					<div>
						<span class="font-bold text-sm">{m.matrix_effect_palettes()}</span>
						<p class="text-xs text-base-content/70">{m.matrix_effect_palettes_desc()}</p>
					</div>
					<div class="grid grid-cols-2 gap-2 2xl:grid-cols-3">
						{#each colorPresets as preset}
							<button
								type="button"
								class="btn btn-sm h-auto min-h-0 justify-start gap-3 px-3 py-2 {activeColorPresetId ===
								preset.id
									? 'btn-primary'
									: 'btn-outline btn-primary'}"
								disabled={effectControlsDisabled}
								aria-pressed={activeColorPresetId === preset.id}
								onclick={() => applyColorPreset(preset)}
							>
								<span class="flex items-center gap-1">
									{#each preset.colors as color}
										<span
											class="h-3 w-3 rounded-full border border-base-content/20"
											style={`background-color: ${toMatrixHexColor(color)};`}
										></span>
									{/each}
								</span>
								<span>{preset.label}</span>
							</button>
						{/each}
					</div>
				</ContentBox>

				<ContentBox class="flex items-center justify-between">
					<div>
						<span class="font-bold text-sm">{m.matrix_base_color()}</span>
						<p class="text-xs text-base-content/70">{m.matrix_color_desc()}</p>
					</div>
					<div class="flex flex-col gap-1">
						<div class="flex items-center gap-2 justify-end">
							<span class="font-mono text-xs opacity-70">1: {hexColor}</span>
							<FormColorInput
								value={hexColor}
								disabled={effectControlsDisabled}
								ariaLabel={m.matrix_effect_color_primary({ locale: i18n.languageTag })}
								oninput={handleColorChange}
							/>
						</div>
						<div class="flex items-center gap-2 justify-end">
							<span class="font-mono text-xs opacity-70">2: {hexColor2}</span>
							<FormColorInput
								value={hexColor2}
								disabled={effectControlsDisabled}
								ariaLabel={m.matrix_effect_color_secondary({ locale: i18n.languageTag })}
								oninput={handleColorChange2}
							/>
						</div>
						<div class="flex items-center gap-2 justify-end">
							<span class="font-mono text-xs opacity-70">3: {hexColor3}</span>
							<FormColorInput
								value={hexColor3}
								disabled={effectControlsDisabled}
								ariaLabel={m.matrix_effect_color_tertiary({ locale: i18n.languageTag })}
								oninput={handleColorChange3}
							/>
						</div>
					</div>
				</ContentBox>
			</div>
		{/if}
	</div>
</SettingsCard>
