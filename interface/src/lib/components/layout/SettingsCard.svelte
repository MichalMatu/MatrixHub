<script module lang="ts">
	let settingsCardInstance = 0;
</script>

<script lang="ts">
	import type { Component, Snippet } from 'svelte';
	import BaseCard from './BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { unsavedChanges } from '$lib/stores/unsavedChanges.svelte';
	import { formVariants } from '$lib/styles/design-system';
	import * as m from '$lib/paraglide/messages.js';
	import ArrowBackUp from '~icons/tabler/arrow-back-up';
	import DeviceFloppy from '~icons/tabler/device-floppy';

	let {
		title = undefined,
		icon: Icon = undefined,
		iconClass = 'w-6 h-6',
		actions = undefined,
		children = undefined,
		class: className = '',
		bleed = false,
		hideTitleOnTiny = true,
		hasChanges = false,
		loading = false,
		saving = false,
		disabled = false,
		error = undefined,
		onSave = undefined,
		onReset = undefined,
		dirtySourceId = undefined,
		saveLabel = undefined,
		discardLabel = undefined
	}: {
		title?: string;
		icon?: Component;
		iconClass?: string;
		actions?: Snippet;
		children?: Snippet;
		class?: string;
		bleed?: boolean;
		hideTitleOnTiny?: boolean;
		hasChanges?: boolean;
		loading?: boolean;
		saving?: boolean;
		disabled?: boolean;
		error?: string | null;
		onSave?: () => void | Promise<unknown>;
		onReset?: () => void;
		dirtySourceId?: string;
		saveLabel?: string;
		discardLabel?: string;
	} = $props();

	let saveInFlight = $state(false);
	const fallbackDirtySourceId = `settings-card-${++settingsCardInstance}`;

	const controlsDisabled = $derived(disabled || loading);
	const saveDisabled = $derived(controlsDisabled || saving || saveInFlight || !hasChanges);
	const resetDisabled = $derived(controlsDisabled || saving || saveInFlight || !hasChanges);
	const showActions = $derived(Boolean(onSave || onReset));
	const resolvedDirtySourceId = $derived(dirtySourceId ?? fallbackDirtySourceId);

	$effect(() => {
		if (!showActions) {
			unsavedChanges.clearSource(resolvedDirtySourceId);
			return;
		}

		unsavedChanges.setSourceDirty(resolvedDirtySourceId, hasChanges);
		return () => {
			unsavedChanges.clearSource(resolvedDirtySourceId);
		};
	});

	async function handleSave() {
		if (!onSave || saveDisabled) return;
		saveInFlight = true;
		try {
			await onSave();
		} finally {
			saveInFlight = false;
		}
	}

	function handleReset() {
		if (!onReset || resetDisabled) return;
		onReset();
	}
</script>

<BaseCard {title} icon={Icon} {iconClass} {actions} class={className} {bleed} {hideTitleOnTiny}>
	<div class="flex h-full min-h-0 flex-col">
		<div class="min-w-0">
			{@render children?.()}
		</div>

		{#if error}
			<p class="sr-only" aria-live="assertive">{error}</p>
		{/if}

		{#if showActions}
			<div class={`${formVariants.actions} px-1`}>
				{#if onReset}
					<FormButton
						variant="ghost"
						size="sm"
						icon={ArrowBackUp}
						ariaLabel={discardLabel ?? m.action_discard()}
						title={discardLabel ?? m.action_discard()}
						disabled={resetDisabled}
						onclick={handleReset}
					/>
				{/if}
				{#if onSave}
					<FormButton
						onclick={handleSave}
						disabled={saveDisabled}
						loading={saving || saveInFlight}
						label={saveLabel ?? m.action_save()}
						icon={DeviceFloppy}
					/>
				{/if}
			</div>
		{/if}
	</div>
</BaseCard>
