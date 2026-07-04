<script lang="ts">
	import type { Component, Snippet } from 'svelte';
	import ContentBox from './ContentBox.svelte';

	let {
		icon: Icon = undefined,
		leading = undefined,
		paddingClass = 'px-4 py-2',
		iconClass = 'h-6 w-6 flex-none text-base-content/70',
		iconSlot = undefined,
		label = undefined,
		labelClass = 'font-bold',
		labelAddon = undefined,
		value = undefined,
		valueClass = 'text-sm opacity-75',
		subValue = undefined,
		subValueClass = 'text-xs opacity-50',
		details = undefined,
		actions = undefined,
		class: className = ''
	}: {
		icon?: Component;
		leading?: Snippet;
		paddingClass?: string;
		iconClass?: string;
		iconSlot?: Snippet;
		label?: string;
		labelClass?: string;
		labelAddon?: Snippet;
		value?: string | number;
		valueClass?: string;
		subValue?: string | number;
		subValueClass?: string;
		details?: Snippet;
		actions?: Snippet;
		class?: string;
	} = $props();
</script>

<ContentBox {paddingClass} class="flex items-center space-x-3 {className}">
	{#if leading}
		<div class="flex items-center">
			{@render leading()}
		</div>
	{/if}
	<div class="flex h-10 w-10 items-center justify-center">
		{#if iconSlot}
			{@render iconSlot()}
		{:else if Icon}
			<Icon class={iconClass} />
		{/if}
	</div>
	<div class="flex-1 min-w-0">
		{#if label}
			{#if labelAddon}
				<div class="flex items-center gap-2 min-w-0">
					<div class={labelClass}>{label}</div>
					{@render labelAddon()}
				</div>
			{:else}
				<div class={labelClass}>{label}</div>
			{/if}
		{/if}
		{#if details}
			{@render details()}
		{:else}
			{#if value !== undefined}
				<div class={valueClass}>{value}</div>
			{/if}
			{#if subValue !== undefined}
				<div class={subValueClass}>{subValue}</div>
			{/if}
		{/if}
	</div>
	{#if actions}
		<div class="shrink-0 flex items-center gap-1">
			{@render actions()}
		</div>
	{/if}
</ContentBox>
