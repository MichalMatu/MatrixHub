<script lang="ts">
	import type { Snippet } from 'svelte';

	let {
		children,
		title = undefined,
		paddingClass = 'p-4',
		class: className = ''
	}: {
		children: Snippet;
		title?: string;
		paddingClass?: string;
		class?: string;
	} = $props();
</script>

<div class="rounded-md border border-base-300/50 bg-base-100/25 {paddingClass} {className}">
	{#if title}
		<h4 class="mb-2 text-sm font-semibold text-base-content/70">
			{title}
		</h4>
	{/if}
	{@render children()}
</div>
