<!-- @runes -->
<script lang="ts">
	import { browser } from '$app/environment';
	import { semantic } from '$lib/styles/design-system';
	import { createHtmlId } from '$lib/utils/dom/htmlId';
	import type { HTMLTextareaAttributes } from 'svelte/elements';

	interface Props extends HTMLTextareaAttributes {
		label?: string;
		value?: string;
		error?: string;
		help?: string;
		ariaLabel?: string;
		class?: string;
	}

	let {
		label,
		id = '',
		value = $bindable(''),
		placeholder = '',
		rows = 4,
		required = false,
		disabled = false,
		readonly = false,
		error = '',
		help = '',
		minlength,
		maxlength,
		ariaLabel,
		name,
		class: className = '',
		oninput,
		onchange,
		onkeydown,
		onfocus,
		onblur,
		...restProps
	}: Props = $props();

	let fallbackId = $state('');

	$effect(() => {
		const hasProvidedId = !!id?.trim();
		if (!hasProvidedId && browser && !fallbackId) {
			fallbackId = createHtmlId('textarea');
		}
		if (hasProvidedId && fallbackId) {
			fallbackId = '';
		}
	});

	const resolvedId = $derived(id?.trim() ? id : fallbackId);
	const resolvedName = $derived(name?.trim() ? name : resolvedId);

	const hasError = $derived(!!error);
	const textareaClasses = $derived(
		`textarea textarea-bordered w-full ${hasError ? 'border-error border-2' : ''} ${className}`
	);

	const handleInput = (e: Event & { currentTarget: HTMLTextAreaElement }) => {
		const target = e.currentTarget;
		value = target?.value ?? '';
		oninput?.(e);
	};

	const handleChange = (e: Event & { currentTarget: HTMLTextAreaElement }) => {
		onchange?.(e);
	};

	const handleKeydown = (e: KeyboardEvent & { currentTarget: HTMLTextAreaElement }) => {
		onkeydown?.(e);
	};

	const handleFocus = (e: FocusEvent & { currentTarget: HTMLTextAreaElement }) => {
		onfocus?.(e);
	};

	const handleBlur = (e: FocusEvent & { currentTarget: HTMLTextAreaElement }) => {
		onblur?.(e);
	};
</script>

<div class={semantic.form.field}>
	{#if label}
		<label class={semantic.form.label} for={resolvedId || undefined}>{label}</label>
	{/if}

	<textarea
		id={resolvedId || undefined}
		name={resolvedName || undefined}
		{placeholder}
		{rows}
		{required}
		{disabled}
		{readonly}
		{minlength}
		{maxlength}
		aria-label={ariaLabel || label}
		class={textareaClasses}
		{value}
		{...restProps}
		oninput={handleInput}
		onchange={handleChange}
		onkeydown={handleKeydown}
		onfocus={handleFocus}
		onblur={handleBlur}
	></textarea>

	{#if error}
		<span class={semantic.form.error}>{error}</span>
	{:else if help}
		<span class={semantic.form.help}>{help}</span>
	{/if}
</div>
