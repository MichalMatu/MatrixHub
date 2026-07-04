<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { ApStatus } from '$lib/types/connectivity/ap';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	// Icons
	import AP from '~icons/tabler/access-point';
	import MAC from '~icons/tabler/dna-2';
	import Home from '~icons/tabler/home';
	import Devices from '~icons/tabler/devices';

	interface Props {
		status: ApStatus;
	}

	let { status }: Props = $props();

	const detailIconClass = 'h-6 w-6 flex-none text-base-content/70';

	const apStatusDescription = $derived([
		{
			icon_color: 'text-success',
			description: m.status_active({ locale: i18n.languageTag })
		},
		{
			icon_color: 'text-error',
			description: m.status_inactive({ locale: i18n.languageTag })
		}
	]);
</script>

<BaseCard title={m.menu_wifi_ap({ locale: i18n.languageTag })} icon={AP} class="h-full">
	<div
		class="flex w-full flex-col gap-1"
		transition:slide|local={{ duration: 300, easing: cubicOut }}
	>
		<StatusRow
			icon={AP}
			iconClass={`h-6 w-6 flex-none ${apStatusDescription[status.status].icon_color}`}
			label={m.ap_stat_status({ locale: i18n.languageTag })}
			value={apStatusDescription[status.status].description}
		/>

		<StatusRow
			icon={Home}
			iconClass={detailIconClass}
			label={m.ap_stat_ip({ locale: i18n.languageTag })}
			value={status.ip_address}
		/>

		<StatusRow
			icon={MAC}
			iconClass={detailIconClass}
			label={m.ap_stat_mac({ locale: i18n.languageTag })}
			value={status.mac_address}
		/>

		<StatusRow
			icon={Devices}
			iconClass={detailIconClass}
			label={m.ap_stat_clients({ locale: i18n.languageTag })}
			value={status.station_num}
		/>
	</div>
</BaseCard>
