<script lang="ts">
	import { Modal } from '$lib/components';
	import { FormButton } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import type { ExtendedHealthDiagnostics } from '$lib/types/system/systemStatusSnapshot';
	import { formatDuration } from '$lib/utils';

	import Network from '~icons/tabler/network';
	import X from '~icons/tabler/x';

	interface Props {
		open?: boolean;
		health: ExtendedHealthDiagnostics | null;
	}

	let { open = $bindable(false), health }: Props = $props();

	function formatBool(
		value: boolean | undefined,
		trueLabel: string = m.action_yes({ locale: i18n.languageTag }),
		falseLabel: string = m.action_no({ locale: i18n.languageTag })
	) {
		return value ? trueLabel : falseLabel;
	}

	function boolBadgeClass(value: boolean | undefined, neutralWhenUndefined: boolean = false) {
		if (value === undefined && neutralWhenUndefined) return 'badge-ghost';
		return value ? 'badge-success' : 'badge-error';
	}

	function formatOptional(value: string | number | undefined | null, fallback: string = '—') {
		if (value === undefined || value === null || value === '') return fallback;
		return String(value);
	}

	function formatReason(value: string | undefined) {
		if (!value) return '—';
		const trimmed = value.trim();
		return trimmed.length > 0 ? trimmed : '—';
	}

	function formatTimestampMs(value: number | undefined, uptimeMs: number | undefined) {
		if (!value) return '—';
		const uptime = uptimeMs ?? 0;
		if (uptime > value) {
			return m.network_diag_time_ago(
				{ duration: formatDuration(Math.floor((uptime - value) / 1000)) },
				{ locale: i18n.languageTag }
			);
		}
		return m.network_diag_time_uptime(
			{ seconds: Math.floor(value / 1000) },
			{ locale: i18n.languageTag }
		);
	}

	// The top summary card intentionally shows "current / peak", not
	// "current / configured limit". This helps spot leak-like drift over time
	// without suggesting that the right-hand side is a hard capacity ceiling.
	function formatSocketCount(active: number | undefined, peak: number | undefined) {
		return `${active ?? 0} / ${peak ?? 0}`;
	}

	function formatPayloadBytes(value: number | undefined, count: number | undefined) {
		if (!count || value === undefined || value === null) return '—';
		return `${value} B`;
	}
</script>

<Modal
	isOpen={open}
	onClose={() => (open = false)}
	widthClass="w-full max-w-3xl"
	paddingClass="p-5"
>
	<div class="mb-2 flex items-center justify-between">
		<h3 class="flex items-center gap-2 text-lg font-bold">
			<Network class="h-5 w-5 text-primary" />
			{m.network_diag_title({ locale: i18n.languageTag })}
		</h3>
		<FormButton
			label=""
			icon={X}
			class="btn-ghost btn-xs"
			onclick={() => (open = false)}
			ariaLabel={m.action_close({ locale: i18n.languageTag })}
			title={m.action_close({ locale: i18n.languageTag })}
		/>
	</div>

	{#if health}
		<div class="mb-4 grid grid-cols-2 gap-2 text-center lg:grid-cols-4">
			<div class="rounded-box bg-base-200 px-3 py-2">
				<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
					{m.network_diag_summary_sta_state({ locale: i18n.languageTag })}
				</div>
				<div class="mt-1 text-sm font-semibold">{formatOptional(health.wifi.state)}</div>
			</div>
			<div class="rounded-box bg-base-200 px-3 py-2">
				<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
					{m.network_diag_summary_wifi_mode({ locale: i18n.languageTag })}
				</div>
				<div class="mt-1 text-sm font-semibold">{formatOptional(health.wifi.mode)}</div>
			</div>
			<div class="rounded-box bg-base-200 px-3 py-2">
				<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
					{m.network_diag_summary_forwarding({ locale: i18n.languageTag })}
				</div>
				<div class="mt-1">
					<span class="badge badge-sm {boolBadgeClass(health.forwarding?.ready)}">
						{formatBool(
							health.forwarding?.ready,
							m.network_diag_status_ready({ locale: i18n.languageTag }),
							m.network_diag_status_blocked({ locale: i18n.languageTag })
						)}
					</span>
				</div>
			</div>
			<div class="rounded-box bg-base-200 px-3 py-2">
				<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
					{m.network_diag_summary_sockets({ locale: i18n.languageTag })}
				</div>
				<div class="mt-1 text-sm font-semibold">
					{formatSocketCount(health.http?.activeClients, health.http?.peakClients)}
				</div>
			</div>
		</div>

		<div class="grid grid-cols-1 gap-3 lg:grid-cols-2">
			<div class="rounded-box bg-base-200 p-4">
				<div class="mb-3 text-sm font-bold uppercase tracking-wider opacity-70">
					{m.network_diag_section_wifi({ locale: i18n.languageTag })}
				</div>
				<div class="grid grid-cols-[minmax(0,1fr)_auto] gap-x-4 gap-y-2 text-sm">
					<div class="opacity-70">
						{m.network_diag_field_connected({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">{formatBool(health.wifi.connected)}</div>
					<div class="opacity-70">{m.wifi_stat_ssid({ locale: i18n.languageTag })}</div>
					<div class="font-mono">{formatOptional(health.wifi.ssid)}</div>
					<div class="opacity-70">{m.network_diag_field_sta_ip({ locale: i18n.languageTag })}</div>
					<div class="font-mono">{formatOptional(health.wifi.ip)}</div>
					<div class="opacity-70">{m.wifi_stat_rssi({ locale: i18n.languageTag })}</div>
					<div class="font-mono">{formatOptional(health.wifi.rssi, '—')} dBm</div>
					<div class="opacity-70">
						{m.network_diag_field_last_disconnect_reason({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">{formatOptional(health.wifi.lastDisconnectReason, '0')}</div>
					<div class="opacity-70">
						{m.network_diag_field_configured_mode({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">{formatOptional(health.wifi.configuredMode)}</div>
					<div class="opacity-70">
						{m.network_diag_field_ap_active({ locale: i18n.languageTag })}
					</div>
					<div>
						<span class="badge badge-sm {boolBadgeClass(health.wifi.apActive)}">
							{formatBool(health.wifi.apActive)}
						</span>
					</div>
					<div class="opacity-70">
						{m.network_diag_field_disconnected_since({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatTimestampMs(health.wifi.disconnectedSinceMs, health.runtime?.uptimeMs)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_stable_connected_since({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatTimestampMs(health.wifi.stableConnectedSinceMs, health.runtime?.uptimeMs)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_last_ip_change({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatTimestampMs(health.wifi.lastIpChangeMs, health.runtime?.uptimeMs)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_last_recovery_reason({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono break-all text-right">
						{formatReason(health.wifi.lastRecoveryReason)}
					</div>
				</div>
			</div>

			<div class="rounded-box bg-base-200 p-4">
				<div class="mb-3 text-sm font-bold uppercase tracking-wider opacity-70">
					{m.network_diag_section_access_point({ locale: i18n.languageTag })}
				</div>
				<div class="grid grid-cols-[minmax(0,1fr)_auto] gap-x-4 gap-y-2 text-sm">
					<div class="opacity-70">{m.network_diag_field_active({ locale: i18n.languageTag })}</div>
					<div>
						<span class="badge badge-sm {boolBadgeClass(health.ap?.active)}">
							{formatBool(health.ap?.active)}
						</span>
					</div>
					<div class="opacity-70">{m.network_diag_field_ap_ip({ locale: i18n.languageTag })}</div>
					<div class="font-mono">{formatOptional(health.ap?.ip)}</div>
					<div class="opacity-70">
						{m.network_diag_field_stations({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">{formatOptional(health.ap?.stationNum, '0')}</div>
				</div>

				<div class="my-4 border-t border-base-300"></div>

				<div class="mb-3 text-sm font-bold uppercase tracking-wider opacity-70">
					{m.network_diag_section_forwarding({ locale: i18n.languageTag })}
				</div>
				<div class="grid grid-cols-[minmax(0,1fr)_auto] gap-x-4 gap-y-2 text-sm">
					<div class="opacity-70">{m.network_diag_field_ready({ locale: i18n.languageTag })}</div>
					<div>
						<span class="badge badge-sm {boolBadgeClass(health.forwarding?.ready)}">
							{formatBool(health.forwarding?.ready)}
						</span>
					</div>
					<div class="opacity-70">
						{m.network_diag_field_requires_static_ip({ locale: i18n.languageTag })}
					</div>
					<div>
						<span
							class="badge badge-sm {boolBadgeClass(health.forwarding?.requiresStaticIp, true)}"
						>
							{formatBool(health.forwarding?.requiresStaticIp)}
						</span>
					</div>
					<div class="opacity-70">
						{m.network_diag_field_static_ip_configured({ locale: i18n.languageTag })}
					</div>
					<div>
						<span
							class="badge badge-sm {boolBadgeClass(health.forwarding?.savedStaticIpConfigured)}"
						>
							{formatBool(health.forwarding?.savedStaticIpConfigured)}
						</span>
					</div>
					<div class="opacity-70">
						{m.network_diag_field_saved_static_ip({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">{formatOptional(health.wifi.savedStaticIp)}</div>
					<div class="opacity-70">
						{m.network_diag_field_static_ip_matches_current({ locale: i18n.languageTag })}
					</div>
					<div>
						<span class="badge badge-sm {boolBadgeClass(health.forwarding?.savedStaticIpMatches)}">
							{formatBool(health.forwarding?.savedStaticIpMatches)}
						</span>
					</div>
					<div class="opacity-70">
						{m.network_diag_field_https_port({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">{formatOptional(health.forwarding?.httpsPort, '443')}</div>
				</div>
			</div>

			<div class="rounded-box bg-base-200 p-4 lg:col-span-2">
				<div class="mb-3 text-sm font-bold uppercase tracking-wider opacity-70">
					{m.network_diag_section_http_ws({ locale: i18n.languageTag })}
				</div>
				<div class="mb-3 text-xs opacity-65">
					{m.network_diag_http_description({ locale: i18n.languageTag })}
				</div>
				<div class="grid grid-cols-2 gap-2 text-sm md:grid-cols-3 xl:grid-cols-6">
					<div class="rounded-box bg-base-100 px-3 py-2">
						<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
							{m.network_diag_card_active_sockets({ locale: i18n.languageTag })}
						</div>
						<div class="mt-1 font-mono text-lg">{health.http?.activeClients ?? 0}</div>
					</div>
					<div class="rounded-box bg-base-100 px-3 py-2">
						<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
							{m.network_diag_card_peak_sockets({ locale: i18n.languageTag })}
						</div>
						<div class="mt-1 font-mono text-lg">{health.http?.peakClients ?? 0}</div>
					</div>
					<div class="rounded-box bg-base-100 px-3 py-2">
						<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
							{m.network_diag_card_socket_opens_closes({ locale: i18n.languageTag })}
						</div>
						<div class="mt-1 font-mono text-lg">
							{health.http?.opens ?? 0} / {health.http?.closes ?? 0}
						</div>
					</div>
					<div class="rounded-box bg-base-100 px-3 py-2">
						<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
							{m.network_diag_card_forced_ws_removals({ locale: i18n.languageTag })}
						</div>
						<div class="mt-1 font-mono text-lg">{health.http?.wsForcedRemovals ?? 0}</div>
					</div>
					<div class="rounded-box bg-base-100 px-3 py-2">
						<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
							{m.network_diag_card_ws_queue_drops({ locale: i18n.languageTag })}
						</div>
						<div class="mt-1 font-mono text-lg">{health.http?.wsQueueDrops ?? 0}</div>
					</div>
					<div class="rounded-box bg-base-100 px-3 py-2">
						<div class="text-[10px] font-bold uppercase tracking-wider opacity-60">
							{m.network_diag_card_ws_heap_fallbacks({ locale: i18n.languageTag })}
						</div>
						<div class="mt-1 font-mono text-lg">{health.http?.wsHeapFallbacks ?? 0}</div>
					</div>
				</div>

				<div class="mt-4 grid grid-cols-[minmax(0,1fr)_auto] gap-x-4 gap-y-2 text-sm">
					<div class="opacity-70">
						{m.network_diag_field_last_open({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatTimestampMs(health.http?.lastOpenMs, health.runtime?.uptimeMs)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_last_close({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatTimestampMs(health.http?.lastCloseMs, health.runtime?.uptimeMs)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_last_queue_drop({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatTimestampMs(health.http?.lastWsQueueDropMs, health.runtime?.uptimeMs)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_last_dropped_payload({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatPayloadBytes(health.http?.lastWsQueueDropPayload, health.http?.wsQueueDrops)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_last_heap_fallback({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatTimestampMs(health.http?.lastWsHeapFallbackMs, health.runtime?.uptimeMs)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_last_heap_fallback_payload({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatPayloadBytes(
							health.http?.lastWsHeapFallbackPayload,
							health.http?.wsHeapFallbacks
						)}
					</div>
					<div class="opacity-70">
						{m.network_diag_field_max_heap_fallback_payload({ locale: i18n.languageTag })}
					</div>
					<div class="font-mono">
						{formatPayloadBytes(
							health.http?.maxWsHeapFallbackPayload,
							health.http?.wsHeapFallbacks
						)}
					</div>
				</div>
			</div>
		</div>
	{:else}
		<div class="alert alert-warning">
			<span>{m.network_diag_unavailable({ locale: i18n.languageTag })}</span>
		</div>
	{/if}

	{#snippet actions()}
		<div class="flex w-full justify-end">
			<FormButton
				label={m.action_close({ locale: i18n.languageTag })}
				icon={X}
				onclick={() => (open = false)}
				class="btn-neutral"
			/>
		</div>
	{/snippet}
</Modal>
