<script lang="ts">
	import UdpSettings from './UdpSettings.svelte';
	import UdpTest from './UdpTest.svelte';
	import { useUdpSettings } from './useUdpSettings.svelte';
	import { useUdpTest } from './useUdpTest.svelte';
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const udpState = useUdpSettings({ shouldLoad: () => canManage });
	const udpTest = useUdpTest(() => ({
		host: udpState.savedSettings?.host ?? udpState.settings.host,
		port: udpState.savedSettings?.port ?? udpState.settings.port
	}));
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<UdpSettings {udpState} />
		{#if udpState.canTest && !udpState.loading}
			<UdpTest {udpState} {udpTest} />
		{/if}
	</GridLayout>
</AdminAccessGate>
