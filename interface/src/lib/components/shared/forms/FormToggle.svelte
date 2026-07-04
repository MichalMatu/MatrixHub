<script lang="ts">
	// Toggle moved into shared/forms so all basic form controls now come from one
	// primitive layer and one public import path through $lib/components.
	let {
		checked = $bindable(false),
		label = undefined,
		description = undefined,
		disabled = false,
		ariaLabel = undefined,
		title = undefined,
		class: className = '',
		plain = false,
		onchange
	}: {
		checked?: boolean;
		label?: string;
		description?: string;
		disabled?: boolean;
		ariaLabel?: string;
		title?: string;
		class?: string;
		plain?: boolean;
		onchange?: (_: Event) => void;
	} = $props();
</script>

{#snippet inner()}
	{#if label || description}
		<div>
			{#if label}
				<div class="font-bold text-sm">{label}</div>
			{/if}
			{#if description}
				<div class="text-xs opacity-70">
					{description}
				</div>
			{/if}
		</div>
	{/if}
	<input
		type="checkbox"
		bind:checked
		class="toggle toggle-primary toggle-sm"
		{disabled}
		aria-label={ariaLabel || label || undefined}
		{title}
		{onchange}
	/>
{/snippet}

{#if plain}
	<div class={className}>
		{@render inner()}
	</div>
{:else}
	<div
		class="flex items-center justify-between gap-3 rounded-md border border-base-300/60 bg-base-100/30 px-4 py-3 {className}"
	>
		{@render inner()}
	</div>
{/if}
