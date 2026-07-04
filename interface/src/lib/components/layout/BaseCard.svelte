<script lang="ts">
	import type { Component, Snippet } from 'svelte';
	import * as m from '$lib/paraglide/messages.js';

	/**
	 * Base card component for consistent card styling across the application.
	 * Provides standardized header with optional icon and actions.
	 *
	 * @param bleed - When true, uses reduced horizontal padding (px-2) so content
	 *   like charts can use more width while still having breathing room.
	 *
	 * @example
	 * ```svelte
	 * <BaseCard title={m.settings_title()} icon={Settings}>
	 *   {#snippet actions()}
	 *     				<button class="btn btn-primary btn-sm">{m.action_save()}</button>
	 *   {/snippet}
	 *   {#snippet children()}
	 *     <!-- Card content here -->
	 *   {/snippet}
	 * </BaseCard>
	 * ```
	 */

	import ChevronDown from '~icons/tabler/chevron-down';

	let {
		title = undefined,
		icon: Icon = undefined,
		iconClass = 'w-6 h-6',
		actions = undefined,
		children,
		class: className = '',
		collapsible = false,
		collapsed = $bindable(false),
		bleed = false,
		hideTitleOnTiny = true
	}: {
		title?: string;
		icon?: Component;
		iconClass?: string;
		actions?: Snippet;
		children: Snippet;
		class?: string;
		collapsible?: boolean;
		collapsed?: boolean;
		bleed?: boolean;
		hideTitleOnTiny?: boolean;
	} = $props();

	function toggle() {
		if (collapsible) {
			collapsed = !collapsed;
		}
	}
</script>

<div class="card bg-base-200 card-shadow-base min-w-0 {className}">
	<div class="card-body {bleed ? 'px-2 py-3' : 'p-4'}">
		{#if title || Icon || actions || collapsible}
			<!-- svelte-ignore a11y_click_events_have_key_events -->
			<!-- svelte-ignore a11y_no_static_element_interactions -->
			<div
				class="flex items-center justify-between mb-3 {collapsible
					? 'cursor-pointer select-none hover:opacity-80 transition-opacity'
					: ''}"
				onclick={toggle}
				role="button"
				tabindex="0"
				onkeydown={(e) => {
					if (collapsible && (e.key === 'Enter' || e.key === ' ')) {
						e.preventDefault();
						toggle();
					}
				}}
			>
				<h2 class="card-title text-sm flex items-center gap-2 min-w-0">
					{#if collapsible}
						<div class="w-8 h-8 flex items-center justify-center shrink-0">
							<ChevronDown
								class="w-6 h-6 transition-transform duration-300 {!collapsed ? 'rotate-180' : ''}"
							/>
						</div>
					{/if}
					{#if Icon}
						<Icon class={iconClass} />
					{/if}
					<span class="{hideTitleOnTiny ? 'max-[360px]:hidden' : ''} truncate"
						>{title || m.settings_title()}</span
					>
				</h2>
				{#if actions}
					<div class="shrink-0" onclick={(e) => e.stopPropagation()}>
						{@render actions()}
					</div>
				{/if}
			</div>
		{/if}

		{#if !collapsible || !collapsed}
			{@render children()}
		{/if}
	</div>
</div>
