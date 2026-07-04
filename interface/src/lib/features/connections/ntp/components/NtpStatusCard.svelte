<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { NTPStatus, NTPSettings } from '$lib/types/connectivity/ntp';
	import { formatLongDateTime, formatUTCDateTime } from '$lib/utils/format/timeFormat';
	import { i18n } from '$lib/i18n.svelte';

	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';

	// Icons
	import NTP from '~icons/tabler/clock-check';
	import Server from '~icons/tabler/server';
	import Clock from '~icons/tabler/clock';
	import UTC from '~icons/tabler/clock-pin';

	import Refresh from '~icons/tabler/refresh';

	interface Props {
		status: NTPStatus;
		settings?: NTPSettings | null;
		onRefresh?: () => void;
	}

	let { status, settings = null, onRefresh }: Props = $props();

	const detailIconClass = 'h-6 w-6 flex-none text-base-content/70';
	const statusIconClass = (active: boolean) =>
		`h-6 w-6 flex-none ${active ? 'text-success' : 'text-error'}`;
</script>

<BaseCard title={m.ntp_status_title({ locale: i18n.languageTag })} icon={Clock}>
	{#snippet actions()}
		{#if onRefresh}
			<FormButton
				label=""
				icon={Refresh}
				class="btn-ghost btn-xs btn-circle"
				onclick={onRefresh}
				aria-label={m.action_refresh_status({ locale: i18n.languageTag })}
				title={m.action_refresh_status({ locale: i18n.languageTag })}
			/>
		{/if}
	{/snippet}
	<div
		class="flex w-full flex-col gap-1"
		transition:slide|local={{ duration: 300, easing: cubicOut }}
	>
		<StatusRow
			icon={NTP}
			iconClass={statusIconClass(status.status === 1)}
			label={m.ntp_label_status({ locale: i18n.languageTag })}
			value={status.status === 1
				? m.ntp_status_active({ locale: i18n.languageTag })
				: m.ntp_status_inactive({ locale: i18n.languageTag })}
		/>

		<StatusRow
			icon={Server}
			iconClass={detailIconClass}
			label={m.ntp_label_server({ locale: i18n.languageTag })}
			value={status.server}
		/>

		<!-- Empty timestamps are a valid bootstrap state now: ntpStatus degrades
		     to "" instead of returning HTTP 500 while time/TZ are still settling. -->
		<StatusRow
			icon={Clock}
			iconClass={detailIconClass}
			label={m.ntp_label_local({ locale: i18n.languageTag })}
			value={status.local_time ? formatLongDateTime(status.local_time, settings?.tz_label) : '--'}
		/>

		<!-- Keep the same placeholder semantics for UTC so the card stays calm
		     while the backend recovers time instead of flashing "Invalid date". -->
		<StatusRow
			icon={UTC}
			iconClass={detailIconClass}
			label={m.ntp_label_utc({ locale: i18n.languageTag })}
			value={status.utc_time ? formatUTCDateTime(status.utc_time) : '--'}
		/>
	</div>
</BaseCard>
