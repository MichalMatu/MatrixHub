<script lang="ts">
	import WiFi from '~icons/tabler/wifi';
	import WiFi0 from '~icons/tabler/wifi-0';
	import WiFi1 from '~icons/tabler/wifi-1';
	import WiFi2 from '~icons/tabler/wifi-2';

	let { showDBm = false, rssi_dbm = 0, class: className = '' } = $props();
</script>

{#if showDBm}
	<div class="indicator">
		<span class="indicator-item indicator-start badge badge-neutral badge-xs">
			{rssi_dbm} dBm
		</span>
		{#if rssi_dbm >= -55}
			<WiFi class={className} />
		{:else if rssi_dbm >= -75}
			<WiFi2 class={className} />
		{:else if rssi_dbm >= -85}
			<WiFi1 class={className} />
		{:else}
			<WiFi0 class={className} />
		{/if}
	</div>
{:else if rssi_dbm >= -55}
	<WiFi class={className} />
{:else if rssi_dbm >= -75}
	<WiFi2 class={className} />
{:else if rssi_dbm >= -85}
	<WiFi1 class={className} />
{:else}
	<WiFi0 class={className} />
{/if}
