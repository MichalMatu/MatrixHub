<script lang="ts">
	import { type ModalProps } from 'svelte-modals';
	import Modal from '$lib/components/Modal.svelte';
	import FormButton from '$lib/components/shared/forms/FormButton.svelte';
	import { onMount, onDestroy } from 'svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { closeModal } from '$lib/utils/ui/modal';
	import Power from '~icons/tabler/reload';
	import Check from '~icons/tabler/check';
	import X from '~icons/tabler/x';
	import { useRestartSequence, type RestartStage } from './useRestartSequence.svelte';

	interface Props extends ModalProps {
		title?: string;
		/** Async function to save settings (called first) */
		onSave: () => Promise<unknown>;
		/** Optional function to trigger restart via API (uses default PowerApiService if not provided) */
		triggerRestart?: () => Promise<void>;
		/** Use hygiene sleep instead of full restart (faster ~100ms vs ~5s) */
		useSleepInsteadOfRestart?: boolean;
		/** Message shown during save */
		savingMessage?: string;
		/** Message shown during restart */
		restartingMessage?: string;
		/** Message shown while waiting for device */
		waitingMessage?: string;
		/** Message shown on success */
		successMessage?: string;
	}

	let {
		isOpen,
		title = m.restart_dialog_title_restarting(),
		onSave,
		triggerRestart,
		useSleepInsteadOfRestart = false,
		savingMessage = m.restart_stage_saving(),
		restartingMessage = m.restart_stage_restarting(),
		waitingMessage = m.restart_stage_waiting(),
		successMessage = m.restart_stage_success()
	}: Props = $props();

	const restart = useRestartSequence({
		onSave: () => onSave(),
		getTriggerRestart: () => triggerRestart,
		useSleepInsteadOfRestart: () => useSleepInsteadOfRestart
	});

	const stageMessages: Record<RestartStage, string> = $derived({
		saving: savingMessage,
		restarting: restartingMessage,
		waiting: waitingMessage,
		success: successMessage,
		error: restart.errorMessage
	});

	function handleClose() {
		restart.destroy();
		closeModal();
	}

	onMount(() => {
		void restart.runRestartSequence();
	});

	onDestroy(() => {
		restart.destroy();
	});
</script>

<Modal
	{isOpen}
	onClose={() => {}}
	{title}
	widthClass="w-full max-w-xs sm:max-w-sm"
	paddingClass="p-6"
	closeOnOutsideClick={false}
	showCloseButton={false}
	titleClass="text-base-content text-center text-xl font-bold"
	bodyClass="flex flex-col items-center gap-4 py-4"
>
	{#if restart.stage === 'error'}
		<div class="text-error">
			<X class="h-12 w-12" />
		</div>
		<p class="text-center text-error">{stageMessages[restart.stage]}</p>
		<FormButton label={m.action_close()} class="btn-primary" onclick={handleClose} />
	{:else if restart.stage === 'success'}
		<div class="text-success">
			<Check class="h-12 w-12" />
		</div>
		<p class="text-center text-success">{stageMessages[restart.stage]}</p>
		<p class="text-base-content/60 text-sm">{m.restart_reloading()}</p>
	{:else}
		<span class="loading loading-spinner loading-lg text-primary"></span>
		<p class="text-center">{stageMessages[restart.stage]}</p>
		{#if restart.stage === 'waiting'}
			<p class="text-base-content/60 text-sm">
				<Power class="inline h-4 w-4" />
				{m.restart_waiting_timer({ seconds: restart.elapsedSeconds })}
			</p>
		{/if}
	{/if}
</Modal>
