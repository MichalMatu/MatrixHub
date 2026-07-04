<script lang="ts">
	import Refresh from '~icons/tabler/refresh';
	import X from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Temperature from '~icons/tabler/temperature';
	import Droplet from '~icons/tabler/droplet';
	import Battery4 from '~icons/tabler/battery-4';
	import Battery3 from '~icons/tabler/battery-3';
	import Battery2 from '~icons/tabler/battery-2';
	import Battery1 from '~icons/tabler/battery-1';
	import Battery from '~icons/tabler/battery';
	import Signal from '~icons/tabler/antenna-bars-5';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { Modal } from '$lib/components';
	import { FormButton } from '$lib/components/shared/forms';

	interface ScannedDevice {
		mac: string;
		temp: number;
		humid: number;
		batt: number;
		rssi: number;
		lastSeen: number;
		alias?: string;
		saved?: boolean;
	}

	import type { BleSettings, BleSensorConfig } from '$lib/types/connectivity/ble';
	import { normalizeMac } from '$lib/utils/ble';

	interface Props {
		isOpen: boolean;
		isScanning: boolean;
		scanTimeLeft: number;
		scanResults: Record<string, ScannedDevice>;
		savedSettings: BleSettings | null;
		onStartScan: () => void;
		onCloseScan: () => void;
		onAddDevice: (_device: ScannedDevice) => void;
	}

	let {
		isOpen,
		isScanning,
		scanTimeLeft,
		scanResults,
		savedSettings,
		onStartScan,
		onCloseScan,
		onAddDevice
	}: Props = $props();

	function getBatteryIcon(level: number) {
		if (level >= 90) return Battery4;
		if (level >= 60) return Battery3;
		if (level >= 40) return Battery2;
		if (level >= 10) return Battery1;
		return Battery;
	}
</script>

<Modal {isOpen} onClose={onCloseScan}>
	<!-- Title Content for Modal -->
	<div class="flex justify-between items-center mb-2">
		<h2 class="text-base-content text-start text-2xl font-bold">
			{m.ble_scan_title({ locale: i18n.languageTag })}
		</h2>
		{#if isScanning}
			<span class="badge badge-neutral font-mono animate-pulse">{scanTimeLeft}s</span>
		{/if}
	</div>

	<div class="divider my-1"></div>

	<!-- Main Content -->
	<div class="overflow-y-auto overflow-x-hidden flex-1 min-h-[300px] pr-1">
		{#if Object.keys(scanResults).length === 0}
			{#if isScanning}
				<div class="flex flex-col items-center justify-center p-8 h-full opacity-50 text-center">
					<span class="loading loading-ring loading-lg text-primary mb-2"></span>
					<p class="text-sm">{m.ble_scanning_msg({ locale: i18n.languageTag })}</p>
				</div>
			{:else}
				<div class="flex flex-col items-center justify-center p-8 h-full opacity-50 text-center">
					<p>{m.ble_scan_no_devices({ locale: i18n.languageTag })}</p>
				</div>
			{/if}
		{:else}
			<ul class="menu w-full p-0">
				{#each Object.values(scanResults) as device (device.mac)}
					{@const isPaired =
						device.saved ||
						savedSettings?.sensors?.some(
							(s: BleSensorConfig) => normalizeMac(s.mac) === normalizeMac(device.mac)
						)}
					{@const BatteryIconScan = getBatteryIcon(device.batt)}
					<li>
						<div
							class="bg-base-200 rounded-btn my-1 flex items-center space-x-3 p-2 transition-colors hover:bg-base-100 active:bg-base-200"
						>
							<!-- Icon -->
							<div
								class="mask mask-hexagon bg-primary/10 text-primary h-10 w-10 shrink-0 flex items-center justify-center"
							>
								{#if isPaired}
									<Check class="w-6 h-6" />
								{:else}
									<Temperature class="w-6 h-6" />
								{/if}
							</div>

							<!-- Info Middle -->
							<div class="flex-1 min-w-0 flex flex-col justify-center">
								<div class="font-bold font-mono text-sm leading-tight flex items-center gap-2">
									{device.mac}
								</div>
								<div class="text-xs opacity-70 flex flex-wrap gap-x-3 mt-1">
									<!-- Humidity -->
									<span
										class="flex items-center gap-1"
										title={m.dashboard_humid({ locale: i18n.languageTag })}
									>
										<Droplet class="w-3 h-3" />
										{device.humid}%
									</span>
									<!-- Battery -->
									<span
										class="flex items-center gap-1"
										title={m.tooltip_battery({ locale: i18n.languageTag })}
									>
										<BatteryIconScan class="w-3 h-3" />
										{device.batt}%
									</span>
									<!-- RSSI -->
									<span
										class="flex items-center gap-1"
										title={m.tooltip_signal({ locale: i18n.languageTag })}
									>
										<Signal class="w-3 h-3" />
										{device.rssi} dBm
									</span>
								</div>
							</div>

							<!-- Value Right -->
							<div class="flex flex-col items-end justify-center pl-2">
								<div class="font-bold text-lg text-primary leading-none">
									{device.temp.toFixed(1)}°C
								</div>

								{#if !isPaired}
									<FormButton
										label={m.ble_scan_btn_add({ locale: i18n.languageTag })}
										class="btn-xs btn-primary mt-1"
										onclick={(e) => {
											e.stopPropagation();
											onAddDevice(device);
										}}
									/>
								{:else}
									<span class="text-[10px] uppercase font-bold text-success mt-1"
										>{m.ble_scan_added({ locale: i18n.languageTag })}</span
									>
								{/if}
							</div>
						</div>
					</li>
				{/each}
			</ul>
		{/if}
	</div>

	<div class="divider my-2"></div>

	<!-- Actions -->
	{#snippet actions()}
		<div class="flex w-full flex-wrap justify-end gap-2">
			<FormButton
				label={m.action_close({ locale: i18n.languageTag })}
				icon={X}
				class="btn-neutral"
				onclick={onCloseScan}
			/>
			<FormButton
				label={m.ble_scan_again({ locale: i18n.languageTag })}
				icon={Refresh}
				class="btn-primary"
				disabled={isScanning}
				loading={isScanning}
				onclick={onStartScan}
			/>
		</div>
	{/snippet}
</Modal>
