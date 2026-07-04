<script lang="ts">
	import PowerSettings from './PowerSettings.svelte';
	import { AdminAccessGate } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
</script>

<AdminAccessGate allow={canManage}>
	<PowerSettings />
</AdminAccessGate>
