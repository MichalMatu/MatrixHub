<script lang="ts">
	import { onMount } from 'svelte';
	import { Spinner } from '$lib/components';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { CardHeader } from '$lib/components/common';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import Keyboard from '~icons/tabler/keyboard';
	import * as m from '$lib/paraglide/messages.js';
	import KeyboardPanel from '$lib/components/keyboard/KeyboardPanel.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
	import { KeyboardApiService } from '$lib/services/api/integrations/KeyboardApiService';
	import { useKeyboardSettings } from '$lib/features/usb/keyboard/useKeyboardSettings.svelte';
	import { fade } from 'svelte/transition';

	type StatusKind = 'success' | 'error' | 'info' | '';
	let lastStatus = $state('');
	let lastStatusKind = $state<StatusKind>('');
	const apiClient = useApiClient();
	const session = useSessionAccess();
	const settings = useKeyboardSettings(() => apiClient.createService(KeyboardApiService));

	function handleEnabledToggle(event: Event) {
		const nextEnabled = (event.currentTarget as HTMLInputElement).checked;
		settings.confirmEnabledChange(nextEnabled);
	}

	onMount(() => {
		void settings.fetchSettings();
	});
</script>

{#snippet headerActions()}
	{#if lastStatus}
		<span
			class="text-xs max-w-[200px] truncate text-right"
			class:text-error={lastStatusKind === 'error'}
			class:text-success={lastStatusKind === 'success'}
			class:opacity-70={lastStatusKind === 'info' || lastStatusKind === ''}
			title={lastStatus}
		>
			{lastStatus}
		</span>
	{/if}

	{#if settings.savedConfig}
		<label class="flex items-center gap-2 shrink-0">
			<!-- The header toggle is now the single entry point for enable/disable.
			     It opens the restart confirmation immediately instead of staging a draft
			     change that still needs a separate Save action. -->
			<input
				type="checkbox"
				class="toggle toggle-primary toggle-sm"
				checked={settings.localEnabled}
				disabled={settings.saving}
				aria-label={m.keyboard_enable_label()}
				title={m.keyboard_enable_label()}
				onchange={handleEnabledToggle}
			/>
		</label>
	{/if}
{/snippet}

<BaseCard class="mb-3 md:mb-4">
	<CardHeader title={m.keyboard_title()} icon={Keyboard} actions={headerActions} />

	<div in:fade>
		{#if !settings.savedConfig && !settings.error}
			<div class="flex justify-center items-center py-8">
				<Spinner />
			</div>
		{:else}
			<div class="flex flex-col gap-3">
				{#if settings.error}
					<div class="alert alert-error alert-soft">
						<span>{settings.error}</span>
					</div>
				{/if}

				{#if settings.savedConfig?.enabled}
					<KeyboardPanel token={session.bearerToken} bind:lastStatus bind:lastStatusKind />
				{:else}
					<ContentBox class="flex flex-col gap-2">
						<div class="font-semibold text-sm">{m.keyboard_disabled_notice_title()}</div>
						<div class="text-sm opacity-80">{m.keyboard_disabled_notice_body()}</div>
					</ContentBox>
				{/if}
			</div>
		{/if}
	</div>
</BaseCard>
