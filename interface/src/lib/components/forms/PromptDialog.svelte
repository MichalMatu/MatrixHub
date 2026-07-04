<script lang="ts">
	import { untrack } from 'svelte';
	import { type ModalProps } from 'svelte-modals';
	import Modal from '$lib/components/Modal.svelte';
	import { FormButton, FormInput } from '$lib/components/shared/forms';
	import { closeModal } from '$lib/utils/ui/modal';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props extends ModalProps {
		title: string;
		message?: string;
		defaultValue?: string;
		placeholder?: string;
		maxlength?: number;
		asciiOnly?: boolean;
		onConfirm: (value: string) => void;
		onCancel?: () => void;
		labels?: {
			cancel: { label: string; icon: typeof Cancel };
			confirm: { label: string; icon: typeof Cancel };
		};
	}

	let {
		isOpen,
		title,
		message = '',
		defaultValue = '',
		placeholder = '',
		maxlength,
		asciiOnly = false,
		onConfirm,
		onCancel,
		labels
	}: Props = $props();

	let value = $state(untrack(() => defaultValue));

	const fallbackLabels = $derived({
		cancel: { label: m.action_cancel({ locale: i18n.languageTag }), icon: Cancel },
		confirm: { label: m.action_save({ locale: i18n.languageTag }), icon: Check }
	});

	const finalLabels = $derived({
		cancel: { ...fallbackLabels.cancel, ...labels?.cancel },
		confirm: { ...fallbackLabels.confirm, ...labels?.confirm }
	});

	function handleConfirm() {
		closeModal();
		onConfirm?.(value);
	}

	function handleCancel() {
		onCancel?.();
		closeModal();
	}

	function handleKeydown(e: KeyboardEvent) {
		if (e.key === 'Enter') {
			handleConfirm();
		}
	}

	function handleInput(e: Event) {
		const target = e.target as HTMLInputElement;
		let next = target.value;
		if (asciiOnly) {
			next = next.replace(/[^\x20-\x7E]/g, '');
		}
		if (typeof maxlength === 'number') {
			next = next.slice(0, maxlength);
		}
		value = next;
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
	{#if message}
		<p class="text-base-content mb-3 text-start break-words whitespace-pre-line">{message}</p>
	{/if}

	<!-- svelte-ignore a11y_autofocus -->
	<FormInput
		type="text"
		{placeholder}
		{maxlength}
		bind:value
		onkeydown={handleKeydown}
		oninput={handleInput}
		autofocus
	/>

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
