<!-- @runes -->

<script lang="ts">
	import { getButtonClasses } from '$lib/styles/design-system';
	import type { HTMLButtonAttributes } from 'svelte/elements';

	interface Props extends HTMLButtonAttributes {
		variant?:
			| 'primary'
			| 'secondary'
			| 'ghost'
			| 'danger'
			| 'success'
			| 'warning'
			| 'accent'
			| 'neutral'
			| 'icon';
		size?: 'xs' | 'sm' | 'md' | 'lg';
		type?: 'button' | 'submit' | 'reset';
		disabled?: boolean;
		loading?: boolean;
		label?: string;
		icon?: import('svelte').Component;
		ariaLabel?: string;
		title?: string;
		class?: string;
		children?: import('svelte').Snippet;
		onclick?: (_event: MouseEvent) => void;
		'aria-pressed'?: boolean | 'true' | 'false' | 'mixed';
	}

	let {
		variant = 'primary',
		size = 'md',
		type = 'button',
		disabled = false,
		loading = false,
		label,
		icon: Icon,
		onclick,
		ariaLabel,
		title,
		class: additionalClasses = '',
		children,
		'aria-pressed': ariaPressed,
		...restProps
	}: Props = $props();

	const buttonClasses = $derived(`${getButtonClasses(variant, size)} ${additionalClasses}`.trim());
	const isDisabled = $derived(disabled || loading);
</script>

<button
	{type}
	disabled={isDisabled}
	{title}
	aria-label={ariaLabel || (!label ? title || undefined : undefined)}
	aria-pressed={ariaPressed}
	class={buttonClasses}
	{onclick}
	{...restProps}
>
	{#if loading}
		<span class="loading loading-spinner loading-sm"></span>
	{:else if Icon}
		<Icon class="h-4 w-4" />
	{/if}
	{#if label}
		{label}
	{/if}
	{@render children?.()}
</button>
