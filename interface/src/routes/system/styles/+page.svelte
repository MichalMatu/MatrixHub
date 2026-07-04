<script lang="ts">
	import { PageWrapper } from '$lib/components/layout';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { AVAILABLE_THEMES, themeStore } from '$lib/stores/theme.svelte';
	import {
		FormButton,
		FormInput,
		FormRange,
		FormSelect,
		FormToggle
	} from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let previewText = $state('');
	let previewToggle = $state(true);

	// Helpers for slider bindings (removing/adding units)
	let radiusValue = $state(parseFloat(themeStore.settings.borderRadius));
	let animValue = $state(parseFloat(themeStore.settings.animationSpeed));
	let fontValue = $state(parseFloat(themeStore.settings.fontSize || '100'));

	function updateRadius(v: number) {
		radiusValue = v;
		themeStore.setRadius(`${v}rem`);
	}

	function updateAnim(v: number) {
		animValue = v;
		themeStore.setAnimationSpeed(`${v}s`);
	}

	function updateFont(v: number) {
		fontValue = v;
		themeStore.setFontSize(`${v}%`);
	}

	function getThemeName(t: string): string {
		const mapping: Record<string, (args: { locale: string }) => string> = {
			business: m.theme_business,
			corporate: m.theme_corporate,
			night: m.theme_night,
			black: m.theme_black,
			luxury: m.theme_luxury,
			dracula: m.theme_dracula,
			forest: m.theme_forest,
			coffee: m.theme_coffee,
			dim: m.theme_dim,
			sunset: m.theme_sunset,
			halloween: m.theme_halloween,
			synthwave: m.theme_synthwave,
			light: m.theme_light,
			nord: m.theme_nord,
			retro: m.theme_retro
		};
		return mapping[t] ? mapping[t]({ locale: i18n.languageTag }) : t;
	}

	$effect(() => {
		radiusValue = parseFloat(themeStore.settings.borderRadius);
		animValue = parseFloat(themeStore.settings.animationSpeed);
		fontValue = parseFloat(themeStore.settings.fontSize || '100');
	});
</script>

{#snippet radiusDescription()}
	<div class="flex justify-between text-xs opacity-50 mt-1 w-full">
		<span>{m.styles_square({ locale: i18n.languageTag })}</span>
		<span>{m.styles_round({ locale: i18n.languageTag })}</span>
	</div>
{/snippet}

{#snippet animDescription()}
	<div class="flex justify-between text-xs opacity-50 mt-1 w-full">
		<span>{m.styles_instant({ locale: i18n.languageTag })}</span>
		<span>{m.styles_slow({ locale: i18n.languageTag })}</span>
	</div>
{/snippet}

{#snippet fontDescription()}
	<div class="flex justify-between text-xs opacity-50 mt-1 w-full">
		<span>{m.styles_compact({ locale: i18n.languageTag })}</span>
		<span>{m.styles_large({ locale: i18n.languageTag })}</span>
	</div>
{/snippet}

<PageWrapper>
	<div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
		<SettingsCard
			title={m.styles_title({ locale: i18n.languageTag })}
			class="lg:col-span-2"
			hasChanges={themeStore.hasChanges}
			onSave={() => themeStore.save()}
			onReset={() => themeStore.reset()}
			dirtySourceId="theme-settings"
		>
			{#snippet children()}
				<div class="grid gap-4">
					<div class="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-6 gap-3">
						{#each AVAILABLE_THEMES as theme}
							<button
								type="button"
								class="btn btn-outline h-auto py-3 flex flex-col gap-2 relative overflow-hidden"
								class:btn-active={themeStore.settings.theme === theme}
								onclick={() => themeStore.setTheme(theme)}
							>
								<div
									data-theme={theme}
									class="w-full h-8 rounded bg-base-100 border border-base-content/20 flex items-center justify-center gap-1 p-1 pointer-events-none"
								>
									<div class="w-2 h-2 rounded-full bg-primary"></div>
									<div class="w-2 h-2 rounded-full bg-secondary"></div>
									<div class="w-2 h-2 rounded-full bg-accent"></div>
									<div class="w-2 h-2 rounded-full bg-neutral"></div>
								</div>
								<span class="text-xs font-semibold capitalize">{getThemeName(theme)}</span>
							</button>
						{/each}
					</div>

					<div class="grid gap-3 lg:grid-cols-3">
						<ContentBox>
							<FormRange
								label={m.styles_radius({ locale: i18n.languageTag })}
								value={radiusValue}
								min={0}
								max={2}
								step={0.125}
								suffix="r"
								description={radiusDescription}
								oninput={(e: Event) =>
									updateRadius(parseFloat((e.target as HTMLInputElement).value))}
							/>
						</ContentBox>

						<ContentBox>
							<FormRange
								label={m.styles_animation({ locale: i18n.languageTag })}
								rangeClass="range-secondary"
								value={animValue}
								min={0}
								max={1}
								step={0.1}
								suffix="s"
								description={animDescription}
								oninput={(e: Event) => updateAnim(parseFloat((e.target as HTMLInputElement).value))}
							/>
						</ContentBox>

						<ContentBox>
							<FormRange
								label={m.styles_font_size({ locale: i18n.languageTag })}
								rangeClass="range-accent"
								value={fontValue}
								min={85}
								max={115}
								step={5}
								suffix="%"
								description={fontDescription}
								oninput={(e: Event) => updateFont(parseFloat((e.target as HTMLInputElement).value))}
							/>
						</ContentBox>
					</div>
				</div>
			{/snippet}
		</SettingsCard>

		<BaseCard title={m.styles_preview_title({ locale: i18n.languageTag })}>
			{#snippet children()}
				<div class="grid gap-1">
					<div class="grid grid-cols-2 gap-2">
						<FormButton
							label={m.styles_primary({ locale: i18n.languageTag })}
							class="btn-primary"
						/>
						<FormButton
							label={m.styles_secondary({ locale: i18n.languageTag })}
							class="btn-secondary"
						/>
						<FormButton label={m.styles_accent({ locale: i18n.languageTag })} class="btn-accent" />
						<FormButton label={m.styles_ghost({ locale: i18n.languageTag })} class="btn-ghost" />
					</div>

					<FormInput
						label={m.styles_preview_input({ locale: i18n.languageTag })}
						placeholder={m.styles_preview_input_placeholder({ locale: i18n.languageTag })}
						bind:value={previewText}
					/>

					<div class="rounded-box bg-base-100 p-4 flex flex-wrap items-center gap-3">
						<FormToggle
							label={m.styles_toggle_me({ locale: i18n.languageTag })}
							bind:checked={previewToggle}
							plain
							class="flex items-center gap-4"
						/>
						<input type="checkbox" class="checkbox checkbox-primary" checked />
						<input type="radio" class="radio radio-primary" checked />
						<span class="loading loading-spinner loading-md text-primary"></span>
						<span class="loading loading-dots loading-md text-secondary"></span>
					</div>

					<div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
						<FormSelect
							label={m.styles_select_option({ locale: i18n.languageTag })}
							bind:value={previewText}
							options={[
								{ value: '', label: m.styles_select_placeholder({ locale: i18n.languageTag }) },
								{ value: '1', label: m.styles_select_opt1({ locale: i18n.languageTag }) },
								{ value: '2', label: m.styles_select_opt2({ locale: i18n.languageTag }) },
								{ value: '3', label: m.styles_select_opt3({ locale: i18n.languageTag }) }
							]}
						/>

						<div class="flex flex-col gap-2 justify-center">
							<div class="flex flex-wrap gap-2">
								<div class="badge badge-primary">
									{m.styles_primary({ locale: i18n.languageTag })}
								</div>
								<div class="badge badge-secondary">
									{m.styles_secondary({ locale: i18n.languageTag })}
								</div>
							</div>
							<div class="flex flex-wrap gap-2">
								<div class="badge badge-outline">
									{m.styles_outline({ locale: i18n.languageTag })}
								</div>
								<div class="badge badge-ghost">{m.styles_ghost({ locale: i18n.languageTag })}</div>
							</div>
						</div>
					</div>

					<div class="flex flex-col gap-1">
						<div class="alert alert-info shadow-sm">
							<span>{m.styles_alert_info({ locale: i18n.languageTag })}</span>
						</div>
						<div class="alert alert-success shadow-sm">
							<span>{m.styles_alert_success({ locale: i18n.languageTag })}</span>
						</div>
						<div class="alert alert-error shadow-sm">
							<span>{m.styles_alert_error({ locale: i18n.languageTag })}</span>
						</div>
					</div>
				</div>
			{/snippet}
		</BaseCard>
	</div>
</PageWrapper>
