<!--
	HardwareInfoCard.svelte - Hardware and firmware information display
	Shows CPU, Flash, Temperature, SDK version, etc.
-->
<script lang="ts">
	import { formatBytes, formatTemperature } from '$lib/utils';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';
	import type { SystemInformation } from '$lib/types/system/system';

	import CPU from '~icons/tabler/cpu';
	import CPP from '~icons/tabler/binary';
	import Speed from '~icons/tabler/activity';
	import Flash from '~icons/tabler/device-sd-card';
	import Temperature from '~icons/tabler/temperature';
	import SDK from '~icons/tabler/sdk';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		systemInfo: SystemInformation;
	}

	let { systemInfo }: Props = $props();

	const statIconClass = 'h-6 w-6 flex-none text-base-content/70';

	function getResetReasonText(reason: number): string {
		switch (reason) {
			case 1:
				return m.reset_reason_poweron({ locale: i18n.languageTag });
			case 2:
				return m.reset_reason_ext({ locale: i18n.languageTag });
			case 3:
				return m.reset_reason_sw({ locale: i18n.languageTag });
			case 4:
				return m.reset_reason_panic({ locale: i18n.languageTag });
			case 5:
				return m.reset_reason_int_wdt({ locale: i18n.languageTag });
			case 6:
				return m.reset_reason_task_wdt({ locale: i18n.languageTag });
			case 7:
				return m.reset_reason_wdt({ locale: i18n.languageTag });
			case 8:
				return m.reset_reason_deepsleep({ locale: i18n.languageTag });
			case 9:
				return m.reset_reason_brownout({ locale: i18n.languageTag });
			case 10:
				return m.reset_reason_sdio({ locale: i18n.languageTag });
			default:
				return m.reset_reason_unknown({ locale: i18n.languageTag }) + ` (${reason})`;
		}
	}
</script>

<BaseCard title={m.status_hw_title({ locale: i18n.languageTag })} icon={CPU}>
	<div class="flex w-full flex-col gap-1">
		<!-- Core Temperature -->
		<StatusRow
			icon={Temperature}
			iconClass={statIconClass}
			label={m.status_core_temp({ locale: i18n.languageTag })}
		>
			{#snippet details()}
				<div class="text-sm opacity-75">
					{formatTemperature(systemInfo.core_temp)}
				</div>
				<div class="text-xs opacity-50">
					{m.status_reset_reason({ locale: i18n.languageTag })}: {getResetReasonText(
						systemInfo.cpu_reset_reason
					)}
				</div>
			{/snippet}
		</StatusRow>

		<!-- Processor (Combined Chip & Freq) -->
		<!-- Processor (Combined Chip & Freq) -->
		<StatusRow
			icon={CPU}
			iconClass={statIconClass}
			label={m.status_chip({ locale: i18n.languageTag })}
		>
			{#snippet details()}
				<div class="text-sm opacity-75">
					{systemInfo.cpu_type} Rev {systemInfo.cpu_rev}
				</div>
				<div class="text-xs opacity-50">
					{systemInfo.cpu_freq_mhz}
					{m.unit_mhz({ locale: i18n.languageTag })}
					{systemInfo.cpu_cores === 2
						? m.status_cpu_cores_2({ locale: i18n.languageTag })
						: m.status_cpu_cores_1({ locale: i18n.languageTag })}
				</div>
			{/snippet}
		</StatusRow>

		<!-- Flash Chip -->
		<!-- Flash Chip -->
		<StatusRow
			icon={Flash}
			iconClass={statIconClass}
			label={m.status_flash_chip({ locale: i18n.languageTag })}
			value={`${formatBytes(systemInfo.flash_chip_size)} / ${(
				systemInfo.flash_chip_speed / 1000000
			).toFixed(0)} ${m.unit_mhz({ locale: i18n.languageTag })}`}
		/>

		<!-- MAC Address (New) -->
		<!-- MAC Address (New) -->
		<StatusRow
			icon={Speed}
			iconClass={statIconClass}
			label={m.status_mac_address({ locale: i18n.languageTag })}
			value={systemInfo.mac_address}
			valueClass="text-sm opacity-75 font-mono"
		/>

		<!-- Firmware Version (Enhanced) -->
		<!-- Firmware Version (Enhanced) -->
		<StatusRow
			icon={CPP}
			iconClass={statIconClass}
			label={m.status_firmware({ locale: i18n.languageTag })}
		>
			{#snippet details()}
				<div class="text-sm opacity-75">
					{systemInfo.firmware_version}
				</div>
				{#if systemInfo.compile_date}
					<div class="text-xs opacity-50">
						{systemInfo.compile_date}
						{systemInfo.compile_time}
					</div>
				{/if}
			{/snippet}
		</StatusRow>

		<!-- SDK Version -->
		<!-- SDK Version -->
		<StatusRow
			icon={SDK}
			iconClass={statIconClass}
			label={m.status_sdk({ locale: i18n.languageTag })}
		>
			{#snippet details()}
				<div class="text-sm opacity-75">
					IDF {systemInfo.sdk_version}
				</div>
				<div class="text-xs opacity-50">
					Arduino {systemInfo.arduino_version}
				</div>
			{/snippet}
		</StatusRow>
	</div>
</BaseCard>
