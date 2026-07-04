<script lang="ts">
	import type { MenuItem } from '$lib/features/navigation/menuConfig';

	let {
		item,
		openGroup,
		onItemClick,
		onToggleGroup
	}: {
		item: MenuItem;
		openGroup: string | null;
		onItemClick: (_title: string) => void;
		onToggleGroup: (_title: string | null) => void;
	} = $props();

	const isFeatureEnabled = (feature: boolean | (() => boolean)): boolean => {
		return typeof feature === 'function' ? feature() : feature;
	};

	const isOpen = $derived(openGroup === item.title);
</script>

{#if isFeatureEnabled(item.feature)}
	<li>
		{#if item.submenu}
			<details
				open={isOpen}
				ontoggle={(event) => {
					const details = event.currentTarget;
					if (details.open) {
						onToggleGroup(item.title);
					} else if (openGroup === item.title) {
						onToggleGroup(null);
					}
				}}
			>
				<summary
					class="text-lg font-bold"
					class:opacity-60={Boolean(item.disabledReason)}
					title={item.disabledReason ?? undefined}
				>
					<item.icon class="h-6 w-6" />
					{item.title}
				</summary>
				<ul>
					{#each item.submenu as subMenuItem (subMenuItem.title)}
						{#if isFeatureEnabled(subMenuItem.feature)}
							<li class="hover-bordered">
								{#if subMenuItem.disabledReason}
									<span
										class="text-ml font-bold opacity-60 cursor-not-allowed"
										aria-disabled="true"
										title={subMenuItem.disabledReason}
									>
										<subMenuItem.icon class="h-5 w-5" />{subMenuItem.title}
									</span>
								{:else}
									<a
										href={subMenuItem.href}
										class:bg-base-100={subMenuItem.active}
										class="text-ml font-bold"
										onclick={() => onItemClick(subMenuItem.title)}
									>
										<subMenuItem.icon class="h-5 w-5" />{subMenuItem.title}
									</a>
								{/if}
							</li>
						{/if}
					{/each}
				</ul>
			</details>
		{:else if item.disabledReason}
			<span
				class="text-lg font-bold opacity-60 cursor-not-allowed"
				aria-disabled="true"
				title={item.disabledReason}
			>
				<item.icon class="h-6 w-6" />{item.title}
			</span>
		{:else}
			<a
				href={item.href}
				class:bg-base-100={item.active}
				class="text-lg font-bold"
				onclick={() => onItemClick(item.title)}
			>
				<item.icon class="h-6 w-6" />{item.title}
			</a>
		{/if}
	</li>
{/if}
