<script lang="ts">
	import type { Snippet } from 'svelte';

	let {
		children,
		class: className = '',
		size = 'sm',
		pinRows = false
	}: {
		children: Snippet;
		class?: string;
		size?: 'xs' | 'sm' | 'md' | 'lg' | '';
		pinRows?: boolean;
	} = $props();

	const tableClasses = $derived(
		['table', size ? `table-${size}` : '', pinRows ? 'table-pin-rows' : '', 'w-full', className]
			.filter(Boolean)
			.join(' ')
	);
</script>

<table class={tableClasses}>
	{@render children()}
</table>
