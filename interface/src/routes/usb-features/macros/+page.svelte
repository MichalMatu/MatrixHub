<script lang="ts">
	import { AdminAccessGate } from '$lib/components/layout';
	import MacroCard from '$lib/components/macros/MacroCard.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
</script>

<AdminAccessGate allow={canManage}>
	<MacroCard />
</AdminAccessGate>
