<script lang="ts">
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { Spinner } from '$lib/components/common';
	import IconPalette from '~icons/tabler/palette';
	import IconMoodSmile from '~icons/tabler/mood-smile';
	import IconMarquee from '~icons/tabler/marquee';
	import IconBell from '~icons/tabler/bell';
	import IconEditorModal from '$lib/components/matrix/IconEditorModal.svelte';
	import { type useMatrixSettings } from './useMatrixSettings.svelte';
	import { getMatrixCustomIcons, normalizeMatrixCustomIcons } from './matrixModel';
	import * as m from '$lib/paraglide/messages.js';

	let {
		store,
		canManage = true
	}: {
		store: ReturnType<typeof useMatrixSettings>;
		canManage?: boolean;
	} = $props();

	const modes = [
		{ value: 0, label: m.matrix_mode_solid(), desc: m.matrix_mode_solid_desc(), icon: IconPalette },
		{ value: 1, label: m.matrix_mode_icon(), desc: m.matrix_mode_icon_desc(), icon: IconMoodSmile },
		{
			value: 2,
			label: m.matrix_mode_scroll(),
			desc: m.matrix_mode_scroll_desc(),
			icon: IconMarquee
		}
	];

	let showCustomIcons = $state(false);
	const alarmUsesIcons = $derived(store.settings.alarm_mode === 1);

	function getModeButtonClass(value: number) {
		const selected = store.settings.alarm_mode === value;
		return [
			'flex items-center space-x-3 rounded-md border p-2 text-left transition-colors hover:bg-base-200',
			selected ? 'border-primary bg-base-200 ring-1 ring-primary' : 'border-base-300'
		].join(' ');
	}

	function getModeIconClass(value: number) {
		const selected = store.settings.alarm_mode === value;
		return [
			'rounded-full p-2',
			selected ? 'bg-primary text-primary-content' : 'bg-base-300 text-base-content'
		].join(' ');
	}

	async function handleIconsSave(newIcons: number[][]) {
		if (!canManage) return false;
		store.settings = { ...store.settings, custom_icons: normalizeMatrixCustomIcons(newIcons) };
		return true;
	}
</script>

<SettingsCard
	title={m.matrix_alarm_title()}
	icon={IconBell}
	hasChanges={store.hasChanges}
	loading={store.loading}
	saving={store.saving}
	disabled={!canManage}
	error={store.error}
	onSave={store.saveSettingsNow}
	onReset={store.resetSettings}
	dirtySourceId="matrix-alarm-settings"
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
			<ContentBox title={m.matrix_section_alarm()} class="flex flex-col gap-4">
				<div class="flex flex-col gap-3">
					<span class="font-bold text-sm">{m.matrix_mode_label()}</span>
					<div class="grid grid-cols-1 gap-3 md:grid-cols-3">
						{#each modes as mode}
							<button
								class={getModeButtonClass(mode.value)}
								onclick={() => (store.settings.alarm_mode = mode.value)}
								disabled={!canManage}
								type="button"
								aria-pressed={store.settings.alarm_mode === mode.value}
							>
								<div class={getModeIconClass(mode.value)}>
									<mode.icon class="h-5 w-5" />
								</div>
								<div class="flex min-w-0 flex-col">
									<span class="text-sm font-medium">{mode.label}</span>
									<span class="text-[10px] text-base-content/70">{mode.desc}</span>
								</div>
							</button>
						{/each}
					</div>
				</div>

				<div
					class="rounded-box flex flex-col gap-3 border p-4 sm:flex-row sm:items-center sm:justify-between {alarmUsesIcons
						? 'border-primary/50 bg-primary/5'
						: 'border-base-300 bg-base-200/40'}"
				>
					<div class="min-w-0">
						<div class="flex flex-wrap items-center gap-2">
							<span class="text-sm font-bold">{m.matrix_custom_icons_title()}</span>
							{#if alarmUsesIcons}
								<span class="badge badge-primary badge-sm">{m.matrix_mode_icon()}</span>
							{/if}
						</div>
						<p class="mt-1 text-xs text-base-content/70">{m.matrix_custom_icons_desc()}</p>
					</div>
					<FormButton
						label={m.matrix_edit_icons_btn()}
						class="btn-sm btn-outline w-full sm:w-auto"
						disabled={!canManage}
						onclick={() => (showCustomIcons = true)}
					/>
				</div>

				<IconEditorModal
					isOpen={showCustomIcons}
					onClose={() => (showCustomIcons = false)}
					customIcons={getMatrixCustomIcons(store.settings.custom_icons)}
					onSave={handleIconsSave}
				/>
			</ContentBox>
		{/if}
	</div>
</SettingsCard>
