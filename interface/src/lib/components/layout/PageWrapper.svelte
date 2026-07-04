<script lang="ts">
	import type { Snippet } from 'svelte';

	/**
	 * Standard page wrapper component providing consistent padding and max-width.
	 * Use this to wrap all page content for consistent responsive spacing.
	 *
	 * @example
	 * ```svelte
	 * <PageWrapper>
	 *   <MyPageContent />
	 * </PageWrapper>
	 * ```
	 */
	let { children }: { children: Snippet } = $props();
</script>

<div class="mx-auto w-full min-w-0 max-w-none p-2 sm:p-4 md:p-6 2xl:max-w-7xl">
	{@render children()}
</div>
