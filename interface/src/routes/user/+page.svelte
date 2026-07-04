<script lang="ts">
	import { AdminAccessGate } from '$lib/components/layout';
	import UserPage from '$lib/features/user/UserPage.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
</script>

<AdminAccessGate allow={session.canManage}>
	<UserPage />
</AdminAccessGate>
