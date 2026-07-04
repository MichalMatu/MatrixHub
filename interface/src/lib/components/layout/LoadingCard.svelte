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
		children
	}: {
		title: string;
		icon?: Component;
		loading?: boolean;
		minHeight?: string;
		children?: Snippet;
	} = $props();
</script>

<div class="card bg-base-200 card-shadow-base" style="min-height: {minHeight}">
	<div class="card-body p-4">
		<h2 class="card-title text-lg">
			{#if Icon}
				<Icon class="w-6 h-6" />
			{/if}
			{title}
		</h2>
		{#if loading}
			<div class="flex justify-center items-center py-8">
				<Spinner />
			</div>
		{:else if children}
			{@render children()}
		{/if}
	</div>
</div>
