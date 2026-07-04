<script lang="ts">
	import { AdminAccessGate } from '$lib/components/layout';
	import KeyboardCard from '$lib/components/airmouse/components/KeyboardCard.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
</script>

<AdminAccessGate allow={canManage}>
	<div>
		<KeyboardCard />
	</div>
</AdminAccessGate>
