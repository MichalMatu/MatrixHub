<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
	import type { NotifStatsEventData } from '$lib/stores/system/types';
	import { i18n } from '$lib/i18n.svelte';
	import { calculateWorkerAgeSec } from './workerAge';
	import * as m from '$lib/paraglide/messages.js';

	let { type }: { type: 'webhook' | 'pushover' | 'udp' | 'heartbeat' | 'telegram' } = $props();

	const typeLabel = $derived.by(() => {
		switch (type) {
			case 'webhook':
				return m.notif_type_webhook();
			case 'pushover':
				return m.notif_type_pushover();
			case 'udp':
				return m.notif_type_udp();
			case 'heartbeat':
				return m.notif_type_heartbeat();
			case 'telegram':
				return m.telegram_title();
			default:
				return type;
		}
	});

	let stats = $state<NotifStatsEventData | null>(null);
	let lastEventMs = $state(0);
	let ageSec = $state(0);
	let ageTimer: ReturnType<typeof setInterval> | null = null;

	const notifStatsChannel = createSystemChannelSubscription<never>({
		channel: 'notif_stats',
		onEvent: (event) => {
			if (event.type === 'notif_stats') {
				stats = event.data;
				lastEventMs = Date.now();
			}
		}
	});

	onMount(() => {
		notifStatsChannel.subscribe({ hydrateSnapshot: false });
		ageTimer = setInterval(() => {
			if (lastEventMs > 0) {
				ageSec = Math.floor((Date.now() - lastEventMs) / 1000);
			}
		}, 1000);
	});

	onDestroy(() => {
		notifStatsChannel.destroy();
		if (ageTimer) clearInterval(ageTimer);
	});

	function formatAge(sec: number): string {
		if (sec < 0) return '0s';
		if (sec < 60) return `${sec}s`;
		if (sec < 3600) return `${Math.floor(sec / 60)}m`;
		return `${Math.floor(sec / 3600)}h`;
	}

	function getHttpClass(code: number) {
		if (code >= 200 && code < 300) return 'text-success';
		if (code === 0) return 'opacity-40';
		return 'text-error';
	}
</script>

{#if stats}
	{@const currentUptime = stats.uptimeMs}
	<div class="divider my-2 text-xs opacity-50 uppercase tracking-wider">
		{m.notif_worker_title({ type: typeLabel })}
	</div>
	<div class="grid grid-cols-2 gap-x-4 gap-y-1 text-xs">
		{#if type === 'webhook' || type === 'pushover' || type === 'udp'}
			{@const lastMs =
				type === 'webhook'
					? stats.webhook.lastMs
					: type === 'pushover'
						? stats.pushover.lastMs
						: stats.udp.lastMs}
			{@const age = calculateWorkerAgeSec(currentUptime, lastMs)}
			<span class="opacity-60">{m.notif_worker_last_activity()}</span>
			<span class="text-right font-mono">
				{age >= 0 ? formatAge(age) : '—'}
			</span>
		{:else if type === 'telegram'}
			{@const age = calculateWorkerAgeSec(currentUptime, stats.telegram.lastActivityMs)}
			<span class="opacity-60">{m.telegram_worker_status({ locale: i18n.languageTag })}</span>
			{#if stats.telegram.running}
				<span class="text-right"
					><span class="badge badge-success badge-xs"
						>{m.telegram_worker_running({ locale: i18n.languageTag })}</span
					></span
				>
			{:else if stats.telegram.enabled}
				<span class="text-right"
					><span class="badge badge-warning badge-xs"
						>{m.telegram_worker_starting({ locale: i18n.languageTag })}</span
					></span
				>
			{:else}
				<span class="text-right"
					><span class="badge badge-neutral badge-xs"
						>{m.telegram_worker_disabled({ locale: i18n.languageTag })}</span
					></span
				>
			{/if}

			<span class="opacity-60">{m.notif_worker_last_activity({ locale: i18n.languageTag })}</span>
			<span class="text-right font-mono">
				{age >= 0 ? formatAge(age) : '—'}
			</span>
		{:else}
			<span class="opacity-60">{m.notif_worker_last_sync()}</span>
			<span class="text-right font-mono opacity-40"
				>{lastEventMs > 0 ? formatAge(ageSec) : '—'}</span
			>
		{/if}

		{#if type === 'webhook'}
			<span class="opacity-60">{m.notif_worker_messages()}</span>
			<span class="text-right font-mono text-success"
				>{stats.webhook.sent} <span class="opacity-40">/</span>
				<span class="text-error">{stats.webhook.failed}</span></span
			>
			<span class="opacity-60">{m.notif_worker_http_code()}</span>
			<span class="text-right font-mono {getHttpClass(stats.webhook.httpCode)}"
				>{stats.webhook.httpCode || '—'}</span
			>
		{:else if type === 'pushover'}
			<span class="opacity-60">{m.notif_worker_messages()}</span>
			<span class="text-right font-mono text-success"
				>{stats.pushover.sent} <span class="opacity-40">/</span>
				<span class="text-error">{stats.pushover.failed}</span></span
			>
			<span class="opacity-60">{m.notif_worker_http_code()}</span>
			<span class="text-right font-mono {getHttpClass(stats.pushover.httpCode)}"
				>{stats.pushover.httpCode || '—'}</span
			>
		{:else if type === 'udp'}
			<span class="opacity-60">{m.notif_worker_packets()}</span>
			<span class="text-right font-mono text-success"
				>{stats.udp.sent} <span class="opacity-40">/</span>
				<span class="text-error">{stats.udp.failed}</span></span
			>
		{:else if type === 'telegram'}
			<span class="opacity-60">{m.telegram_worker_messages()}</span>
			<span class="text-right font-mono text-success"
				>{m.telegram_worker_messages_stats({
					in: stats.telegram.messagesProcessed,
					out: stats.telegram.messagesSent
				})}</span
			>
			<span class="opacity-60">{m.telegram_worker_commands()}</span>
			<span class="text-right font-mono">{stats.telegram.commandsExecuted}</span>
			{#if stats.telegram.lastHttpCode > 0}
				<span class="opacity-60">{m.notif_worker_http_code({ locale: i18n.languageTag })}</span>
				<span class="text-right font-mono {getHttpClass(stats.telegram.lastHttpCode)}"
					>{stats.telegram.lastHttpCode}</span
				>
			{/if}
		{/if}
	</div>

	{#if type === 'heartbeat'}
		<div class="mt-2 space-y-2">
			{#each stats.heartbeat as slot, i}
				{@const age = calculateWorkerAgeSec(currentUptime, slot.lastPingMs)}
				{#if slot.lastPingMs > 0 || slot.successCount > 0 || slot.failCount > 0}
					<div class="flex items-center justify-between text-[10px] bg-base-300/30 p-1.5 rounded">
						<span class="font-bold opacity-70">{m.notif_worker_slot({ n: i + 1 })}</span>
						<div class="flex gap-3 font-mono">
							<span class="text-success">{slot.successCount} {m.notif_worker_ok()}</span>
							<span class="text-error">{slot.failCount} {m.notif_worker_err()}</span>
							<span class="opacity-50">{age >= 0 ? formatAge(age) : '—'}</span>
						</div>
					</div>
				{/if}
			{/each}
		</div>
	{/if}
{/if}
