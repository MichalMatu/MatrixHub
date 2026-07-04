<script lang="ts">
	import { type ModalProps } from 'svelte-modals';
	import Modal from '$lib/components/Modal.svelte';
	import FormButton from '$lib/components/shared/forms/FormButton.svelte';
	import { closeModal } from '$lib/utils/ui/modal';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props extends ModalProps {
		title: string;
		message?: string;
		// Use only for trusted markup. Prefer `message` for plain text.
		messageHtml?: string;
		onConfirm: () => void;
		onCancel?: () => void;
		labels?: {
			cancel: { label: string; icon: typeof Cancel };
			confirm: { label: string; icon: typeof Cancel };
		};
	}

	let { isOpen, title, message = '', messageHtml, onConfirm, onCancel, labels }: Props = $props();

	const fallbackLabels = $derived({
		cancel: { label: m.action_cancel({ locale: i18n.languageTag }), icon: Cancel },
		confirm: { label: m.action_confirm({ locale: i18n.languageTag }), icon: Check }
	});

	const finalLabels = $derived({
		cancel: { ...fallbackLabels.cancel, ...labels?.cancel },
		confirm: { ...fallbackLabels.confirm, ...labels?.confirm }
	});

	function handleConfirm() {
		onConfirm?.();
		closeModal();
	}

	function handleCancel() {
		onCancel?.();
		closeModal();
	}
</script>

<Modal
	{isOpen}
	onClose={handleCancel}
	{title}
	widthClass="w-full max-w-[min(30rem,calc(100vw-2rem))]"
	paddingClass="p-0"
	headerClass="px-5 pt-5"
	titleClass="text-base font-semibold text-base-content text-start break-words"
	bodyClass="min-h-0 flex-1 overflow-y-auto px-5 pt-3 text-sm leading-relaxed"
	actionsClass="flex flex-col-reverse gap-2 px-5 pb-5 pt-4 sm:flex-row sm:justify-end"
	showHeaderDivider={false}
	showActionsDivider={false}
>
	{#if messageHtml}
		<p class="text-base-content text-start break-words">
			{@html messageHtml}
		</p>
	{:else}
		<p class="text-base-content text-start break-words whitespace-pre-line">{message}</p>
	{/if}

	{#snippet actions()}
		<FormButton
			label={finalLabels.cancel.label}
			icon={finalLabels.cancel.icon}
			variant="neutral"
			class="w-full sm:w-auto"
			onclick={handleCancel}
		/>
		<FormButton
			label={finalLabels.confirm.label}
			icon={finalLabels.confirm.icon}
			variant="primary"
			class="w-full sm:w-auto"
			onclick={handleConfirm}
		/>
	{/snippet}
</Modal>
