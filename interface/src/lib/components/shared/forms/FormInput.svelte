<!-- @runes -->
<script lang="ts">
	import { browser } from '$app/environment';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { getInputClasses, semantic } from '$lib/styles/design-system';
	import { createHtmlId } from '$lib/utils/dom/htmlId';
	import type { Snippet } from 'svelte';
	import type { HTMLInputAttributes } from 'svelte/elements';
	import Eye from '~icons/tabler/eye';
	import EyeOff from '~icons/tabler/eye-off';

	// This is the canonical input primitive. Supporting text goes through `help`,
	// while password actions and optional suffix snippets stay centralized here.
	interface Props extends HTMLInputAttributes {
		label?: string;
		sizeVariant?: 'xs' | 'sm' | 'md' | 'lg';
		error?: string;
		help?: string;
		constrained?: boolean;
		ariaLabel?: string;
		suffix?: Snippet;
	}

	let {
		label,
		id = '',
		value = $bindable<string | number>(''),
		placeholder = '',
		type = 'text',
		required = false,
		disabled = false,
		readonly = false,
		error = '',
		help = '',
		constrained = false,
		ariaLabel,
		step,
		sizeVariant = 'sm',
		suffix,
		oninput,
		onchange,
		onfocus,
		onblur,
		onpaste,
		class: className = '',
		...restProps
	}: Props = $props();

	let fallbackId = $state('');
	let showPassword = $state(false);

	$effect(() => {
		const hasProvidedId = !!id?.trim();
		if (!hasProvidedId && browser && !fallbackId) {
			fallbackId = createHtmlId('input');
		}
		if (hasProvidedId && fallbackId) {
			fallbackId = '';
		}
	});

	const resolvedId = $derived(id?.trim() ? id : fallbackId);
	const hasError = $derived(!!error);
	const helpText = $derived(help);
	const resolvedType = $derived(type === 'password' && showPassword ? 'text' : type);
	// Password toggles and custom suffixes share one spacing rule so every input variant
	// reserves the same right-side space.
	const paddingClass = $derived(
		type === 'password' && suffix ? 'pr-20' : type === 'password' || suffix ? 'pr-10' : ''
	);
	const inputClasses = $derived(
		`${getInputClasses(hasError, constrained, sizeVariant)} ${paddingClass} ${className}`.trim()
	);

	function parseStep(stepValue: Props['step']): number | null {
		if (stepValue === undefined || stepValue === null) {
			return null;
		}
		if (typeof stepValue === 'number') {
			return Number.isFinite(stepValue) && stepValue > 0 ? stepValue : null;
		}
		const normalized = stepValue.trim().toLowerCase();
		if (!normalized.length || normalized === 'any') {
			return null;
		}
		const parsed = Number(normalized);
		return Number.isFinite(parsed) && parsed > 0 ? parsed : null;
	}

	function computeFractionDigits(stepValue: number | null): number | null {
		if (stepValue === null) {
			return null;
		}
		let digits = 0;
		let scaled = stepValue;
		while (!Number.isInteger(scaled) && digits < 6) {
			scaled *= 10;
			digits += 1;
		}
		return digits;
	}

	function trimTrailingZeros(literal: string): string {
		return literal.replace(/(\.\d*?)0+$/, '$1').replace(/\.$/, '');
	}

	function formatNumberForInput(rawValue: number, rawStep: Props['step']): string {
		if (!Number.isFinite(rawValue)) {
			return '';
		}
		const stepNumber = parseStep(rawStep);
		const fractionDigits = computeFractionDigits(stepNumber);
		const maximumFractionDigits = fractionDigits ?? 6;
		try {
			const formatted = rawValue.toLocaleString('en-US', {
				useGrouping: false,
				minimumFractionDigits: 0,
				maximumFractionDigits
			});
			return fractionDigits === 0 ? formatted : trimTrailingZeros(formatted);
		} catch {
			if (maximumFractionDigits === 0) {
				return Math.round(rawValue).toString();
			}
			return trimTrailingZeros(rawValue.toFixed(maximumFractionDigits));
		}
	}

	const displayValue = $derived(
		type === 'number' && typeof value === 'number'
			? formatNumberForInput(value, step)
			: (value ?? '')
	);

	const handleInput = (e: Event & { currentTarget: HTMLInputElement }) => {
		const target = e.currentTarget;
		if (type === 'number') {
			const raw = target?.value ?? '';
			if (raw === '') {
				value = '';
			} else {
				const parsed = Number(raw);
				value = Number.isNaN(parsed) ? '' : parsed;
			}
		} else {
			value = target?.value ?? '';
		}

		oninput?.(e);
	};

	const handleChange = (e: Event & { currentTarget: HTMLInputElement }) => {
		onchange?.(e);
	};

	const handleFocus = (e: FocusEvent & { currentTarget: HTMLInputElement }) => {
		onfocus?.(e);
	};

	const handleBlur = (e: FocusEvent & { currentTarget: HTMLInputElement }) => {
		onblur?.(e);
	};

	const handlePaste = (e: ClipboardEvent & { currentTarget: HTMLInputElement }) => {
		onpaste?.(e);
	};
</script>

<div class={semantic.form.field}>
	{#if label}
		<label class={semantic.form.label} for={resolvedId || undefined}>{label}</label>
	{/if}

	<div class="relative">
		<input
			id={resolvedId || undefined}
			type={resolvedType}
			{placeholder}
			{required}
			{disabled}
			{readonly}
			{step}
			aria-label={ariaLabel || label || undefined}
			class={inputClasses}
			value={displayValue}
			oninput={handleInput}
			onchange={handleChange}
			onfocus={handleFocus}
			onblur={handleBlur}
			onpaste={handlePaste}
			{...restProps}
		/>

		{#if suffix || type === 'password'}
			<div class="absolute inset-y-0 right-0 flex items-center gap-1 pr-3">
				{#if suffix}
					{@render suffix()}
				{/if}

				{#if type === 'password'}
					<button
						type="button"
						class="flex items-center text-base-content/50 hover:text-base-content focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-primary/40"
						aria-label={showPassword
							? m.action_hide_password({ locale: i18n.languageTag })
							: m.action_show_password({ locale: i18n.languageTag })}
						aria-pressed={showPassword}
						title={showPassword
							? m.action_hide_password({ locale: i18n.languageTag })
							: m.action_show_password({ locale: i18n.languageTag })}
						onclick={() => (showPassword = !showPassword)}
					>
						{#if showPassword}
							<EyeOff class="h-4 w-4" />
						{:else}
							<Eye class="h-4 w-4" />
						{/if}
					</button>
				{/if}
			</div>
		{/if}
	</div>

	{#if error}
		<span class={semantic.form.error}>{error}</span>
	{:else if helpText}
		<span class={semantic.form.help}>{helpText}</span>
	{/if}
</div>
