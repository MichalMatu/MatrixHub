<script lang="ts">
	import Activity from '~icons/tabler/activity';
	import Copy from '~icons/tabler/copy';
	import PlayerPause from '~icons/tabler/player-pause';
	import PlayerPlay from '~icons/tabler/player-play';
	import Trash from '~icons/tabler/trash';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormSelect, FormButton } from '$lib/components/shared/forms';
	import { useLiveTailManagement } from './useLiveTailManagement.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	import { onMount, tick } from 'svelte';
	const session = useSessionAccess();
	const isAdmin = $derived(session.canManage);
	const liveTail = useLiveTailManagement();

	let container: HTMLDivElement | undefined = $state();
	let autoscroll = $state(true);

	onMount(() => {
		return () => {
			liveTail.destroy();
		};
	});

	function handleScroll() {
		if (!container) return;
		const { scrollTop, scrollHeight, clientHeight } = container;
		// Check if we are close to bottom (20px margin)
		autoscroll = scrollHeight - scrollTop - clientHeight < 20;
	}

	$effect(() => {
		if (!isAdmin) {
			liveTail.stop();
			return;
		}

		liveTail.init();
		liveTail.start();

		return () => {
			liveTail.stop();
		};
	});

	// Sticky scroll effect using Svelte 5 runes
	$effect(() => {
		// Track tail changes
		const _ = liveTail.tail;
		if (autoscroll && container) {
			// Use tick to ensure DOM is updated before scrolling
			tick().then(() => {
				container!.scrollTo({ top: container!.scrollHeight, behavior: 'smooth' });
			});
		}
	});
</script>

{#if isAdmin}
	<SettingsCard
		title={`${m.livetail_card_title({ locale: i18n.languageTag })} ${m.livetail_admin_suffix({ locale: i18n.languageTag })}`}
		icon={Activity}
		hasChanges={liveTail.isDirty}
		loading={!liveTail.isLoggingConfigLoaded}
		saving={liveTail.savingConfig}
		onSave={liveTail.saveLoggingSettings}
		onReset={liveTail.resetLoggingSettings}
		dirtySourceId="live-tail-settings"
	>
		{#snippet actions()}
			<div class="flex gap-1">
				<FormButton
					label=""
					icon={Copy}
					class="btn-ghost btn-xs btn-circle"
					onclick={() => liveTail.copyToClipboard()}
					title={m.livetail_copy_tooltip({ locale: i18n.languageTag })}
				/>
				<FormButton
					label=""
					icon={liveTail.isPaused ? PlayerPlay : PlayerPause}
					class="btn-ghost btn-xs btn-circle {liveTail.isPaused ? 'text-warning' : ''}"
					onclick={() => liveTail.togglePause()}
					title={liveTail.isPaused
						? m.action_resume({ locale: i18n.languageTag })
						: m.action_pause({ locale: i18n.languageTag })}
				/>
				<FormButton
					label=""
					icon={Trash}
					class="btn-ghost btn-xs btn-circle text-error cursor-pointer"
					onclick={() => void liveTail.clear()}
					title={m.action_clear({ locale: i18n.languageTag, fallback: 'Clear' })}
				/>
			</div>
		{/snippet}

		<div class="flex flex-col gap-3">
			{#if liveTail.tailError}
				<div class="alert alert-warning text-xs">{liveTail.tailError}</div>
			{/if}
			{#if liveTail.configError}
				<div class="alert alert-warning text-xs">{liveTail.configError}</div>
			{/if}
			<div
				bind:this={container}
				onscroll={handleScroll}
				class="bg-base-300 rounded-lg p-2 max-h-80 overflow-y-auto font-mono text-xs flex flex-col gap-1"
			>
				{#if liveTail.tail.length === 0}
					<div class="opacity-60">{m.livetail_no_logs({ locale: i18n.languageTag })}</div>
				{:else}
					{#each liveTail.tail as line (line.id)}
						{@const rowClass =
							line.level === 'E'
								? 'text-error font-bold'
								: line.level === 'W'
									? 'text-warning'
									: line.level === 'I'
										? 'text-info'
										: line.level === 'D'
											? 'opacity-60'
											: 'opacity-40'}
						<div class="flex gap-2 {rowClass}">
							<span class="w-10 shrink-0 font-semibold">{line.level}</span>
							<span class="w-24 shrink-0 opacity-80">{line.tag}</span>
							<span class="flex-1 break-all">{line.message}</span>
						</div>
					{/each}
				{/if}
			</div>

			<div class="shrink-0">
				<div class="divider my-2">{m.livetail_settings_title({ locale: i18n.languageTag })}</div>

				<div class="grid grid-cols-1 md:grid-cols-2 gap-2 items-end">
					<div class="form-control">
						<FormSelect
							id="log-level"
							label={m.livetail_level_label({ locale: i18n.languageTag })}
							bind:value={liveTail.loggingConfig.level}
							options={liveTail.isLoggingConfigLoaded
								? liveTail.levels.map((l) => ({ value: l, label: l }))
								: [{ value: '', label: '...', disabled: true }]}
							disabled={!liveTail.isLoggingConfigLoaded || liveTail.savingConfig}
							class="select-sm"
						/>
					</div>

					<div class="form-control">
						<div class="label text-xs uppercase opacity-70">
							{m.livetail_ring_buffer({ locale: i18n.languageTag })}
						</div>
						<div class="text-sm py-2 px-3 bg-base-200 rounded">
							{#if liveTail.capacity !== undefined}
								{liveTail.capacity}
							{:else}
								--
							{/if}
							{m.livetail_fixed({ locale: i18n.languageTag })}
						</div>
					</div>
				</div>
			</div>
		</div>
	</SettingsCard>
{/if}
