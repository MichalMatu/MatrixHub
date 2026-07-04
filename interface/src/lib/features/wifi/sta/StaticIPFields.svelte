<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { FormInput } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	import type { KnownNetworkItem } from '$lib/types/connectivity/wifi';

	interface Props {
		network: Partial<KnownNetworkItem>;
		errors: {
			local_ip?: string;
			gateway_ip?: string;
			subnet_mask?: string;
			dns_1?: string;
			dns_2?: string;
		};
		onInputChange?: (field: string, value: string) => void;
		onFieldBlur?: (field: string) => void;
	}

	let { network, errors, onInputChange, onFieldBlur }: Props = $props();

	function handleInput(field: string, event: Event) {
		const val = (event.target as HTMLInputElement).value;
		if (onInputChange) {
			onInputChange(field, val);
		}
	}

	function handleBlur(field: string) {
		onFieldBlur?.(field);
	}
</script>

<div
	class="grid w-full grid-cols-1 content-center gap-4 px-4 sm:grid-cols-2"
	transition:slide={{ duration: 300, easing: cubicOut }}
>
	<div class="sm:col-span-2 text-xs font-bold uppercase opacity-50 border-b pb-1 mt-2">
		{m.wifi_ip_static({ locale: i18n.languageTag })}
	</div>

	<FormInput
		label={m.wifi_stat_ip({ locale: i18n.languageTag })}
		id="local_ip"
		value={network.local_ip || ''}
		oninput={(e) => handleInput('local_ip', e)}
		onblur={() => handleBlur('local_ip')}
		placeholder="192.168.1.100"
		error={errors.local_ip}
		required
	/>

	<FormInput
		label={m.wifi_stat_mask({ locale: i18n.languageTag })}
		id="subnet_mask"
		value={network.subnet_mask || ''}
		oninput={(e) => handleInput('subnet_mask', e)}
		onblur={() => handleBlur('subnet_mask')}
		placeholder="255.255.255.0"
		error={errors.subnet_mask}
		required
	/>

	<FormInput
		label={m.wifi_stat_gateway({ locale: i18n.languageTag })}
		id="gateway_ip"
		value={network.gateway_ip || ''}
		oninput={(e) => handleInput('gateway_ip', e)}
		onblur={() => handleBlur('gateway_ip')}
		placeholder="192.168.1.1"
		error={errors.gateway_ip}
		required
	/>

	<FormInput
		label={`${m.wifi_stat_dns({ locale: i18n.languageTag })} 1`}
		id="dns_ip_1"
		value={network.dns_ip_1 || ''}
		oninput={(e) => handleInput('dns_ip_1', e)}
		onblur={() => handleBlur('dns_ip_1')}
		placeholder="8.8.8.8"
		error={errors.dns_1}
	/>

	<FormInput
		label={`${m.wifi_stat_dns({ locale: i18n.languageTag })} 2`}
		id="dns_ip_2"
		value={network.dns_ip_2 || ''}
		oninput={(e) => handleInput('dns_ip_2', e)}
		onblur={() => handleBlur('dns_ip_2')}
		placeholder="1.1.1.1"
		error={errors.dns_2}
	/>
</div>
