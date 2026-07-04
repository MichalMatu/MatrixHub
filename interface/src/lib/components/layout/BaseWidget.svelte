<script lang="ts">
	import type { Component, Snippet } from 'svelte';
	import { Spinner } from '$lib/components/common';

	/**
	 * Base widget component for dashboard cards.
	 * Provides consistent styling, loading states, and hover effects.
	 *
	 * @example
	 * ```svelte
	 * <BaseWidget
	 *   href="/alarms"
	 *   icon={Bell}
	 *   title={m.menu_alarms()}
	 *   shadowColor={hasActive ? 'error' : 'success'}
	 *   badge={count > 0 ? count.toString() : undefined}
	 *   loading={false}
	 * >
	 *   {#snippet children()}
	 *     <!-- Widget content here -->
	 *   {/snippet}
	 * </BaseWidget>
	 * ```
	 */

	let {
		/** Navigation link URL */
		href,
		/** Icon component to display in header */
		icon: Icon,
		/** Widget title */
		title,
		/** Shadow color variant (error, info, warning, success, primary) */
		shadowColor = undefined,
		/** Optional badge text/number to display after title */
		badge = undefined,
		/** Show loading spinner instead of content */
		loading = false,
		/** Widget content */
		children
	}: {
		href?: string;
		icon: Component;
		title: string;
		shadowColor?: 'error' | 'info' | 'warning' | 'success' | 'primary';
		badge?: string;
		loading?: boolean;
		children: Snippet;
	} = $props();
</script>

{#if href}
	<a
		{href}
		class="card bg-base-200 card-shadow-base cursor-pointer h-full min-h-[180px]"
		class:card-shadow-error={shadowColor === 'error'}
		class:card-shadow-info={shadowColor === 'info'}
		class:card-shadow-warning={shadowColor === 'warning'}
		class:card-shadow-success={shadowColor === 'success'}
		class:card-shadow-primary={shadowColor === 'primary'}
	>
		<div class="card-body p-4 sm:p-5">
			<!-- Header -->
			<div class="flex items-center justify-between gap-2 mb-2 flex-shrink-0 min-w-0">
				<h2 class="card-title text-sm sm:text-base flex items-center gap-2 min-w-0">
					<Icon class="w-5 h-5 flex-shrink-0" />
					<span class="truncate">{title}</span>
					{#if badge}
						<span class="badge badge-ghost badge-xs flex-shrink-0">{badge}</span>
					{/if}
				</h2>
			</div>

			<!-- Content -->
			<div class="relative flex-1 min-h-0">
				{@render children()}
				{#if loading}
					<div
						class="absolute inset-0 z-10 flex items-center justify-center bg-base-200/50 backdrop-blur-[1px] rounded-lg"
					>
						<Spinner />
					</div>
				{/if}
			</div>
		</div>
	</a>
{:else}
	<div
		class="card bg-base-200 card-shadow-base h-full min-h-[180px]"
		class:card-shadow-error={shadowColor === 'error'}
		class:card-shadow-info={shadowColor === 'info'}
		class:card-shadow-warning={shadowColor === 'warning'}
		class:card-shadow-success={shadowColor === 'success'}
		class:card-shadow-primary={shadowColor === 'primary'}
	>
		<div class="card-body p-4 sm:p-5">
			<div class="flex items-center justify-between gap-2 mb-2 flex-shrink-0 min-w-0">
				<h2 class="card-title text-sm sm:text-base flex items-center gap-2 min-w-0">
					<Icon class="w-5 h-5 flex-shrink-0" />
					<span class="truncate">{title}</span>
					{#if badge}
						<span class="badge badge-ghost badge-xs flex-shrink-0">{badge}</span>
					{/if}
				</h2>
			</div>

			<div class="relative flex-1 min-h-0">
				{@render children()}
				{#if loading}
					<div
						class="absolute inset-0 z-10 flex items-center justify-center bg-base-200/50 backdrop-blur-[1px] rounded-lg"
					>
						<Spinner />
					</div>
				{/if}
			</div>
		</div>
	</div>
{/if}
