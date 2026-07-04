<!-- @runes -->
<script lang="ts">
	import { browser } from '$app/environment';
	import { semantic } from '$lib/styles/design-system';
	import { createHtmlId } from '$lib/utils/dom/htmlId';

	interface Props {
		label?: string;
		id?: string;
		files?: FileList | null;
		multiple?: boolean;
		accept?: string;
		required?: boolean;
		disabled?: boolean;
		error?: string;
		help?: string;
		size?: 'sm' | 'md' | 'lg';
		bordered?: boolean;
		ariaLabel?: string;
		inputClass?: string;
		wrapperClass?: string;
		name?: string;
		onchange?: (event: Event) => void;
	}

	let {
		label,
		id = '',
		files = $bindable<FileList | null>(null),
		multiple = false,
		accept,
		required = false,
		disabled = false,
		error = '',
		help = '',
		size = 'sm',
		bordered = true,
		ariaLabel,
		inputClass = '',
		wrapperClass = '',
		name,
		onchange
	}: Props = $props();

	let fallbackId = $state('');
	let inputElement: HTMLInputElement;

	$effect(() => {
		const hasProvidedId = !!id?.trim();
		if (!hasProvidedId && browser && !fallbackId) {
			fallbackId = createHtmlId('file');
		}
		if (hasProvidedId && fallbackId) {
			fallbackId = '';
		}

		// Sync files prop back to input value to allow clearing from parent
		if (!files && inputElement) {
			inputElement.value = '';
		}
	});

	const resolvedId = $derived(id?.trim() ? id : fallbackId);
	const resolvedName = $derived(name?.trim() ? name : resolvedId);

	const hasError = $derived(!!error);
	const inputClasses = $derived(
		[
			'file-input',
			bordered ? 'file-input-bordered' : '',
			size ? `file-input-${size}` : '',
			'w-full',
			hasError ? 'border-error border-2' : '',
			inputClass
		]
			.filter(Boolean)
			.join(' ')
	);

	const handleChange = (event: Event) => {
		const target = event.target as HTMLInputElement | null;
		files = target?.files ?? null;
		if (onchange) onchange(event);
	};
</script>

<div class={`${semantic.form.field} ${wrapperClass}`.trim()}>
	{#if label}
		<label class={semantic.form.label} for={resolvedId || undefined}>{label}</label>
	{/if}

	<input
		bind:this={inputElement}
		type="file"
		id={resolvedId || undefined}
		name={resolvedName || undefined}
		bind:files
		{multiple}
		{accept}
		{required}
		{disabled}
		aria-label={ariaLabel || label}
		class={inputClasses}
		onchange={handleChange}
	/>

	{#if error}
		<span class={semantic.form.error}>{error}</span>
	{:else if help}
		<span class={semantic.form.help}>{help}</span>
	{/if}
</div>
