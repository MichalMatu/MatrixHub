<script lang="ts">
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import PushoverSettings from './PushoverSettings.svelte';
	import PushoverTest from './PushoverTest.svelte';
	import { usePushoverSettings } from './usePushoverSettings.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const pushoverState = usePushoverSettings({ shouldLoad: () => canManage });
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<PushoverSettings {pushoverState} />
		{#if pushoverState.canTest && !pushoverState.loading}
			<PushoverTest />
		{/if}
	</GridLayout>
</AdminAccessGate>
