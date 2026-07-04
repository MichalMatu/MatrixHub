<script lang="ts">
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormColorInput, FormRange, FormToggle } from '$lib/components/shared/forms';
	import { Spinner } from '$lib/components/common';
	import IconGridDots from '~icons/tabler/grid-dots';
	import { type useMatrixSettings } from './useMatrixSettings.svelte';
	import { fromMatrixHexColor, toMatrixHexColor } from './matrixModel';

	import * as m from '$lib/paraglide/messages.js';

	let {
		store,
		canManage = true
	}: {
		store: ReturnType<typeof useMatrixSettings>;
		canManage?: boolean;
	} = $props();

	let menuColorHex = $state('#FFFFFF');

	$effect(() => {
		if (!store.loading) {
			menuColorHex = toMatrixHexColor(store.settings.menu_text_color);
		}
	});

	function handleMenuColorChange(e: Event) {
		if (!canManage) return;
		const target = e.target as HTMLInputElement;
		menuColorHex = target.value;
		store.settings.menu_text_color = fromMatrixHexColor(menuColorHex);
	}
</script>

<SettingsCard
	title={m.matrix_display_title()}
	icon={IconGridDots}
	hasChanges={store.hasChanges}
	loading={store.loading}
	saving={store.saving}
	disabled={!canManage}
	error={store.error}
	onSave={store.saveSettingsNow}
	onReset={store.resetSettings}
	dirtySourceId="matrix-display-settings"
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
			<ContentBox title={m.matrix_section_display()} class="flex flex-col gap-4">
				<FormRange
					label={m.matrix_brightness()}
					min={2}
					max={255}
					step={1}
					disabled={!canManage}
					bind:value={store.settings.brightness}
				/>

				<div class="flex flex-col gap-3">
					<span class="font-bold text-sm">{m.matrix_rotation()}</span>

					<FormToggle
						label={m.matrix_auto()}
						description={m.matrix_auto_desc()}
						bind:checked={store.settings.auto_rotate}
						disabled={!canManage}
						ariaLabel={m.matrix_auto()}
					/>

					<div class="grid grid-cols-2 gap-2 sm:grid-cols-4">
						{#each [0, 1, 2, 3] as rot}
							<button
								type="button"
								class="btn btn-sm flex-1 {store.settings.rotation === rot
									? 'btn-primary'
									: 'btn-outline border-base-300'}"
								disabled={!canManage || store.settings.auto_rotate}
								aria-pressed={store.settings.rotation === rot}
								onclick={() => (store.settings.rotation = rot)}
							>
								{rot === 0 ? '0°' : rot === 1 ? '90°' : rot === 2 ? '180°' : '270°'}
							</button>
						{/each}
					</div>
				</div>
			</ContentBox>

			<ContentBox title={m.matrix_section_menu()} class="flex flex-col gap-4">
				<div class="flex items-center justify-between gap-3">
					<div>
						<span class="font-bold text-sm">{m.matrix_menu_enabled()}</span>
						<p class="text-xs text-base-content/70">{m.matrix_menu_enabled_desc()}</p>
					</div>
					<FormToggle
						label=""
						bind:checked={store.settings.menu_enabled}
						disabled={!canManage}
						ariaLabel={m.matrix_menu_enabled()}
						plain={true}
					/>
				</div>

				<div class="flex items-center justify-between gap-3">
					<div>
						<span class="font-bold text-sm">{m.matrix_menu_text_color()}</span>
						<p class="text-xs text-base-content/70">{m.matrix_menu_text_color_desc()}</p>
					</div>
					<FormColorInput
						class="shrink-0"
						value={menuColorHex}
						disabled={!canManage}
						ariaLabel={m.matrix_menu_text_color()}
						oninput={handleMenuColorChange}
					/>
				</div>

				<FormRange
					label={m.matrix_scroll_speed()}
					min={20}
					max={120}
					step={5}
					suffix="ms"
					disabled={!canManage}
					bind:value={store.settings.menu_scroll_speed}
				/>
			</ContentBox>
		{/if}
	</div>
</SettingsCard>
