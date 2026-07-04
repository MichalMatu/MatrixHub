<script lang="ts">
	import HeartbeatSettings from './HeartbeatSettings.svelte';
	import HeartbeatTest from './HeartbeatTest.svelte';
	import { useHeartbeatSettings } from './useHeartbeatSettings.svelte';
	import { useHeartbeatTest } from './useHeartbeatTest.svelte';
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const heartbeatState = useHeartbeatSettings({ shouldLoad: () => canManage });
	const heartbeatTest = useHeartbeatTest();
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<HeartbeatSettings {heartbeatState} />
		{#if heartbeatState.canTest && !heartbeatState.loading}
			<HeartbeatTest {heartbeatTest} />
		{/if}
	</GridLayout>
</AdminAccessGate>
