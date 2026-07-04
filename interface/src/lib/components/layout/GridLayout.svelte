<script lang="ts">
	import type { Snippet } from 'svelte';

	/**
	 * Responsive grid layout component with configurable column count.
	 * Always single column on mobile, expands to specified columns on md+.
	 *
	 * @example
	 * ```svelte
	 * <GridLayout cols={2}>
	 *   <Card1 />
	 *   <Card2 />
	 * </GridLayout>
	 * ```
	 */
	let {
		/** Number of columns on md+ screens (1-4) */
		cols = 2,
		/** Optional extra top margin */
		marginTop = false,
		children
	}: {
		cols?: 1 | 2 | 3 | 4;
		marginTop?: boolean;
		children: Snippet;
	} = $props();
</script>

<div
	class="grid grid-cols-1 gap-3 md:gap-4"
	class:md:grid-cols-2={cols === 2}
	class:md:grid-cols-3={cols === 3}
	class:md:grid-cols-4={cols === 4}
	class:mt-3={marginTop}
	class:md:mt-4={marginTop}
>
	{@render children()}
</div>
