<script lang="ts">
	import { fly, fade } from 'svelte/transition';
	import { focusTrap } from '$lib/actions/focusTrap';
	import { portal } from '$lib/actions/portal';

	interface Props {
		isOpen: boolean;
		onClose: () => void;
		widthClass?: string;
		paddingClass?: string;
		backdropClass?: string;
		ariaLabel?: string;
		closeOnOutsideClick?: boolean;
		children?: import('svelte').Snippet;
	}

	let {
		isOpen,
		onClose,
		widthClass = 'max-w-lg',
		paddingClass = 'p-4',
		backdropClass = 'bg-black/50 backdrop-blur-sm',
		ariaLabel = undefined,
		closeOnOutsideClick = true,
		children
	}: Props = $props();

	function handleKeydown(e: KeyboardEvent) {
		if (e.key === 'Escape') onClose();
	}
</script>

{#if isOpen}
	<div
		class="fixed inset-0 z-50 flex items-center justify-center p-4 {backdropClass}"
		transition:fade={{ duration: 200 }}
		role="dialog"
		aria-modal="true"
		aria-label={ariaLabel}
		onclick={(e: MouseEvent) => {
			if (closeOnOutsideClick && e.target === e.currentTarget) onClose();
		}}
		onkeydown={handleKeydown}
		tabindex="-1"
		use:focusTrap
		use:portal={'body'}
	>
		<div
			class="border border-base-300/70 bg-base-100 shadow-secondary/30 rounded-box flex max-h-full w-full flex-col justify-between overflow-hidden shadow-xl {widthClass} {paddingClass}"
			transition:fly={{ y: 20, duration: 300 }}
			role="document"
		>
			{@render children?.()}
		</div>
	</div>
{/if}
