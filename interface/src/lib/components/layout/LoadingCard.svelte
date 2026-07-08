<script lang="ts">
	import type { Component, Snippet } from 'svelte';
	import { Spinner } from '$lib/components/common';

	/**
	 * Card component with built-in loading state.
	 * Shows a centered spinner when loading, otherwise renders children.
	 *
	 * @example
	 * ```svelte
	 * <LoadingCard 	title={title || m.menu_alarms()}" icon={Bell} loading={isLoading}>
	 *   {#snippet children()}
	 *     <AlarmList />
	 *   {/snippet}
	 * </LoadingCard>
	 * ```
	 */
	let {
		/** Card title */
		title,
		/** Optional icon component */
		icon: Icon = undefined,
		/** Show loading spinner instead of content */
		loading = false,
		/** Minimum height for the card */
		minHeight = '200px',
		/** Optional card header actions */
		actions = undefined,
		children
	}: {
		title: string;
		icon?: Component;
		loading?: boolean;
		minHeight?: string;
		actions?: Snippet;
		children?: Snippet;
	} = $props();
</script>

<div class="card bg-base-200 card-shadow-base" style="min-height: {minHeight}">
	<div class="card-body p-4">
		<div class="flex items-start justify-between gap-3">
			<h2 class="card-title min-w-0 text-lg">
				{#if Icon}
					<Icon class="w-6 h-6 flex-none" />
				{/if}
				<span class="min-w-0">{title}</span>
			</h2>
			{#if actions}
				<div class="shrink-0">
					{@render actions()}
				</div>
			{/if}
		</div>
		{#if loading}
			<div class="flex justify-center items-center py-8">
				<Spinner />
			</div>
		{:else if children}
			{@render children()}
		{/if}
	</div>
</div>
