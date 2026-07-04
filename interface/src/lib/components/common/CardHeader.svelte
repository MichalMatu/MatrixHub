<script lang="ts">
	import type { Snippet, Component } from 'svelte';

	let {
		title,
		icon: Icon,
		iconClass = '',
		actions
	}: {
		title: string;
		icon: Component<{ class?: string }>;
		iconClass?: string;
		actions?: Snippet;
	} = $props();
</script>

<div class="flex flex-nowrap justify-between items-center gap-2 mb-3">
	<div class="flex items-center gap-2 min-w-0 overflow-hidden">
		<h2 class="card-title text-sm flex items-center gap-2 flex-nowrap min-w-0 w-full">
			<Icon class="w-6 h-6 shrink-0 {iconClass}" />
			<span class="truncate block min-w-0">{title}</span>
		</h2>
	</div>

	{#if actions}
		<div class="flex items-center gap-2 shrink-0">
			{@render actions()}
		</div>
	{/if}
</div>
