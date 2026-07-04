<script lang="ts">
	import Activity from '~icons/tabler/activity';
	import Check from '~icons/tabler/check';
	import Input from '~icons/tabler/login-2';
	import Output from '~icons/tabler/logout-2';
	import Cpu from '~icons/tabler/cpu-2';
	import type { GpioChannelStatus } from '$lib/types/domain/gpio';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	let {
		status = [],
		pinsCount = 0
	}: {
		status?: GpioChannelStatus[];
		pinsCount?: number;
	} = $props();

	const inputCount = $derived(status.filter((channel) => channel.mode === 'input').length);
	const outputCount = $derived(status.filter((channel) => channel.mode === 'output').length);
	const activeCount = $derived(
		status.filter((channel) => channel.configured && channel.logical).length
	);
</script>

<BaseCard title={m.gpio_status_title({ locale: i18n.languageTag })} icon={Activity}>
	<div class="grid grid-cols-1 gap-2 sm:grid-cols-2 xl:grid-cols-4">
		<StatusRow
			icon={Input}
			label={m.gpio_inputs({ locale: i18n.languageTag })}
			labelClass="text-sm font-bold"
			value={inputCount}
			paddingClass="px-3 py-2"
		/>
		<StatusRow
			icon={Output}
			label={m.gpio_outputs({ locale: i18n.languageTag })}
			labelClass="text-sm font-bold"
			value={outputCount}
			paddingClass="px-3 py-2"
		/>
		<StatusRow
			icon={Check}
			label={m.gpio_active_true({ locale: i18n.languageTag })}
			labelClass="text-sm font-bold"
			value={activeCount}
			paddingClass="px-3 py-2"
		/>
		<StatusRow
			icon={Cpu}
			label={m.gpio_safe_pins({ locale: i18n.languageTag })}
			labelClass="text-sm font-bold"
			value={pinsCount}
			paddingClass="px-3 py-2"
		/>
	</div>
</BaseCard>
