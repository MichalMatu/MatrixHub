<script lang="ts">
	import ModalBase from './ModalBase.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import X from '~icons/tabler/x';

	interface Props {
		isOpen: boolean;
		title?: string;
		widthClass?: string; // e.g. "max-w-lg", "max-w-3xl"
		paddingClass?: string;
		backdropClass?: string;
		ariaLabel?: string;
		closeOnOutsideClick?: boolean;
		headerClass?: string;
		titleClass?: string;
		bodyClass?: string;
		actionsClass?: string;
		showHeaderDivider?: boolean;
		showActionsDivider?: boolean;
		showCloseButton?: boolean;
		onClose: () => void;
		children?: import('svelte').Snippet;
		header?: import('svelte').Snippet;
		actions?: import('svelte').Snippet;
		headerActions?: import('svelte').Snippet;
	}

	let {
		isOpen,
		title,
		widthClass = 'max-w-lg',
		paddingClass = 'p-4',
		backdropClass = 'bg-black/50 backdrop-blur-sm',
		ariaLabel = undefined,
		closeOnOutsideClick = true,
		headerClass = '',
		titleClass = 'text-base-content text-start text-2xl font-bold break-words',
		bodyClass = 'overflow-y-auto flex-1',
		actionsClass = 'modal-action mt-4',
		showHeaderDivider,
		showActionsDivider,
		showCloseButton = true,
		onClose,
		children,
		header,
		actions,
		headerActions
	}: Props = $props();

	const hasHeader = $derived(Boolean(header) || Boolean(title));
	const hasActions = $derived(Boolean(actions));
	const shouldShowHeaderDivider = $derived(showHeaderDivider ?? hasHeader);
	const shouldShowActionsDivider = $derived(showActionsDivider ?? hasActions);
	const closeLabel = $derived(m.action_close({ locale: i18n.languageTag }));

	function handleCloseButtonMouseDown(event: MouseEvent) {
		// Keep the click local to the modal close control to avoid visual click-through artifacts.
		event.preventDefault();
		event.stopPropagation();
	}

	function handleCloseButtonClick(event: MouseEvent) {
		event.stopPropagation();
		if (event.currentTarget instanceof HTMLButtonElement) {
			event.currentTarget.blur();
		}
		onClose();
	}
</script>

<ModalBase
	{isOpen}
	{onClose}
	{widthClass}
	{paddingClass}
	{backdropClass}
	ariaLabel={ariaLabel ?? title}
	{closeOnOutsideClick}
>
	{#if header}
		<div class={headerClass}>
			{@render header()}
		</div>
	{:else if title}
		<div class={headerClass}>
			<div class="flex items-center justify-between gap-2">
				<h2 class={titleClass}>
					{title}
				</h2>
				{#if headerActions || showCloseButton}
					<div class="flex items-center gap-2">
						{#if headerActions}
							{@render headerActions()}
						{/if}
						{#if showCloseButton}
							<FormButton
								variant="icon"
								size="sm"
								icon={X}
								ariaLabel={closeLabel}
								title={closeLabel}
								onmousedown={handleCloseButtonMouseDown}
								onclick={handleCloseButtonClick}
								class="-mr-1"
							/>
						{/if}
					</div>
				{/if}
			</div>
		</div>
	{/if}

	{#if hasHeader && shouldShowHeaderDivider}
		<div class="divider my-1"></div>
	{/if}

	<div class={bodyClass}>
		{@render children?.()}
	</div>

	{#if actions}
		{#if shouldShowActionsDivider}
			<div class="divider my-2"></div>
		{/if}
		<div class={actionsClass}>
			{@render actions()}
		</div>
	{/if}
</ModalBase>
