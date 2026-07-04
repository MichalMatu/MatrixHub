<script lang="ts">
	import Cpu from '~icons/tabler/cpu-2';
	import AlertCircle from '~icons/tabler/alert-circle';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { FormSelect } from '$lib/components/shared/forms';
	import { useGpioChannelSelect } from './useGpioChannelSelect.svelte';

	let {
		gpioId = $bindable()
	}: {
		gpioId?: string;
	} = $props();

	$effect(() => {
		if (gpioId === undefined) gpioId = '';
	});

	const gpioState = useGpioChannelSelect(
		() => gpioId,
		(value) => {
			gpioId = value;
		}
	);
</script>

<div class="flex flex-col gap-1">
	<span class="text-xs font-bold flex items-center gap-1 opacity-70 uppercase tracking-wide">
		<Cpu class="w-3 h-3" />
		{m.alarms_field_gpio_channel({ locale: i18n.languageTag })}
	</span>

	{#if gpioState.loading}
		<div class="flex justify-center p-2">
			<span class="loading loading-spinner loading-xs opacity-40"></span>
		</div>
	{:else if gpioState.error}
		<div class="alert alert-error py-1 text-xs px-2 min-h-0 flex items-center gap-2">
			<AlertCircle class="w-3 h-3" />
			<span>{gpioState.error}</span>
		</div>
	{:else if gpioState.channels.length === 0}
		<div class="alert alert-info py-1 text-xs px-2 min-h-0 flex items-center gap-2">
			<Cpu class="w-3 h-3" />
			<span>{m.alarms_no_gpio_inputs({ locale: i18n.languageTag })}</span>
		</div>
	{:else}
		<FormSelect
			bind:value={gpioId}
			options={gpioState.options}
			class={!gpioState.isValidSelection && (gpioId ?? '') !== ''
				? 'select-error select-sm w-full'
				: 'select-sm w-full'}
		/>
		{#if !gpioState.isValidSelection && gpioId !== ''}
			<div class="text-[10px] text-error px-1 mt-0.5">
				{m.alarms_gpio_channel_not_found({ locale: i18n.languageTag })}
			</div>
		{/if}
	{/if}
</div>
