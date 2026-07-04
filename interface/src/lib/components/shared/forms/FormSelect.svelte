<!-- @runes -->
<script lang="ts">
	import { browser } from '$app/environment';
	import { semantic } from '$lib/styles/design-system';
	import { createHtmlId } from '$lib/utils/dom/htmlId';
	import { onMount } from 'svelte';
	import type { HTMLSelectAttributes } from 'svelte/elements';

	// This is the canonical select primitive for the app. It owns the shared label,
	// sizing, focus, and value-mapping behavior used across the interface.
	interface Props extends Omit<HTMLSelectAttributes, 'size'> {
		label?: string;
		options: ReadonlyArray<{ value: string | number; label: string; disabled?: boolean }>;
		help?: string;
		error?: string;
		placeholder?: string;
		size?: 'xs' | 'sm' | 'md' | 'lg';
		nativeSize?: HTMLSelectAttributes['size'];
		bordered?: boolean;
		ariaLabel?: string;
		class?: string;
	}

	let {
		label,
		id = '',
		value = $bindable<string | number>(''),
		options = [] as ReadonlyArray<{ value: string | number; label: string; disabled?: boolean }>,
		required = false,
		disabled = false,
		error = '',
		help = '',
		placeholder = '',
		ariaLabel,
		size = 'sm',
		bordered = true,
		multiple = false,
		name,
		form,
		autofocus = false,
		nativeSize,
		onchange,
		onfocus,
		onblur,
		oninput,
		class: className = '',
		...restProps
	}: Props = $props();

	let fallbackId = $state('');
	let selectElement: HTMLSelectElement | null = null;

	$effect(() => {
		const hasProvidedId = !!id?.trim();
		if (!hasProvidedId && browser && !fallbackId) {
			fallbackId = createHtmlId('select');
		}
		if (hasProvidedId && fallbackId) {
			fallbackId = '';
		}
	});

	onMount(() => {
		if (autofocus && browser && selectElement) {
			queueMicrotask(() => selectElement?.focus());
		}
	});

	$effect(() => {
		if (autofocus && browser && selectElement) {
			selectElement.focus();
		}
	});

	const resolvedId = $derived(id?.trim() ? id : fallbackId);
	const resolvedName = $derived(name?.trim() ? name : resolvedId);

	const hasError = $derived(!!error);
	const selectClasses = $derived(
		[
			'select',
			bordered ? 'select-bordered' : '',
			size ? `select-${size}` : '',
			'w-full',
			hasError ? 'border-error' : '',
			hasError ? 'border-2' : '',
			className
		]
			.filter(Boolean)
			.join(' ')
	);

	// DOM select values always arrive as strings, so we remap them back to the original
	// option values to preserve number-backed selects used by existing settings screens.
	const updateValueFromTarget = (target: HTMLSelectElement | null) => {
		if (!target) {
			return;
		}

		const rawValue = target.value ?? '';
		const matchedOption = options.find((option) => String(option.value) === rawValue);
		value = matchedOption ? matchedOption.value : rawValue;
	};

	const handleInput = (e: Event & { currentTarget: HTMLSelectElement }) => {
		const target = e.currentTarget;
		updateValueFromTarget(target);

		oninput?.(e);
	};

	const handleChange = (e: Event & { currentTarget: HTMLSelectElement }) => {
		updateValueFromTarget(e.currentTarget);
		onchange?.(e);
	};

	const handleFocus = (e: FocusEvent & { currentTarget: HTMLSelectElement }) => {
		onfocus?.(e);
	};

	const handleBlur = (e: FocusEvent & { currentTarget: HTMLSelectElement }) => {
		onblur?.(e);
	};
</script>

<div class={semantic.form.field}>
	{#if label}
		<label class={semantic.form.label} for={resolvedId || undefined}>{label}</label>
	{/if}

	<select
		bind:this={selectElement}
		id={resolvedId || undefined}
		name={resolvedName || undefined}
		{required}
		{disabled}
		{multiple}
		{form}
		size={nativeSize}
		aria-label={ariaLabel || label || undefined}
		class={selectClasses}
		{value}
		{...restProps}
		oninput={handleInput}
		onchange={handleChange}
		onfocus={handleFocus}
		onblur={handleBlur}
	>
		{#if placeholder}
			<option value="" disabled>{placeholder}</option>
		{/if}
		{#each options as option (option.value)}
			<option value={option.value} disabled={option.disabled}>{option.label}</option>
		{/each}
	</select>

	{#if error}
		<span class={semantic.form.error}>{error}</span>
	{:else if help}
		<span class={semantic.form.help}>{help}</span>
	{/if}
</div>
