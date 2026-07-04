<script lang="ts">
	import type { Snippet } from 'svelte';

	// Range lives with the other shared primitives after the form-system cleanup.
	// Keeping sliders here avoids reintroducing a second form component path.
	let {
		value = $bindable(),
		min = 0,
		max = 100,
		step = 1,
		label,
		description,
		disabled = false,
		class: className = '',
		rangeClass = 'range-primary',
		suffix = '',
		valueFormatter = undefined,
		valueClass = '',
		id = undefined,
		ariaLabel = undefined,
		...rest
	}: {
		value: number;
		min?: number;
		max?: number;
		step?: number;
		label?: string;
		description?: string | Snippet;
		disabled?: boolean;
		class?: string;
		rangeClass?: string;
		suffix?: string;
		valueFormatter?: (value: number) => string;
		valueClass?: string;
		id?: string;
		ariaLabel?: string;
		oninput?: (e: Event & { currentTarget: EventTarget & HTMLInputElement }) => void;
		onchange?: (e: Event & { currentTarget: EventTarget & HTMLInputElement }) => void;
	} = $props();

	const displayValue = $derived(
		valueFormatter ? valueFormatter(value) : suffix ? `${value} ${suffix}` : String(value)
	);
</script>

<div class="form-control {className}">
	{#if label}
		<label class="label" for={id}>
			<span class="text-sm font-bold">{label}</span>
		</label>
	{/if}

	<div class="flex items-center gap-2">
		<input
			type="range"
			class="range {rangeClass} range-sm flex-1"
			bind:value
			{min}
			{max}
			{step}
			{disabled}
			{id}
			aria-label={ariaLabel || label || undefined}
			{...rest}
		/>
		<span
			class="text-xs text-right font-mono tabular-nums whitespace-nowrap {valueClass ||
				(suffix ? 'w-20' : 'w-12')}"
		>
			{displayValue}
		</span>
	</div>

	{#if description}
		<div class="label">
			{#if typeof description === 'string'}
				<span class="label-text-alt break-words whitespace-normal block">{description}</span>
			{:else}
				{@render description()}
			{/if}
		</div>
	{/if}
</div>
