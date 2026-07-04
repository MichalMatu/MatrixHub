<script lang="ts">
	import type { HTMLInputAttributes } from 'svelte/elements';

	interface Props extends Omit<HTMLInputAttributes, 'type' | 'value'> {
		value?: string;
		ariaLabel?: string;
		class?: string;
	}

	let {
		value = '#000000',
		disabled = false,
		ariaLabel,
		class: className = '',
		oninput,
		onchange,
		...restProps
	}: Props = $props();

	const inputClasses = $derived(
		`input input-bordered h-8 w-12 cursor-pointer p-0 ${className}`.trim()
	);
</script>

<input
	type="color"
	class={inputClasses}
	{value}
	{disabled}
	aria-label={ariaLabel}
	{oninput}
	{onchange}
	{...restProps}
/>
