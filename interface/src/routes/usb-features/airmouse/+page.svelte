<script lang="ts">
	import { AdminAccessGate } from '$lib/components/layout';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import { useAirMouseManagement } from '$lib/components/airmouse/airMouseState.svelte';
	import AirMouseMain from '$lib/components/airmouse/AirMouseMain.svelte';
	import { fade } from 'svelte/transition';
	import Mouse from '~icons/tabler/mouse';
	import * as m from '$lib/paraglide/messages.js';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const mouseState = useAirMouseManagement({
		shouldInit: () => canManage
	});
</script>

<AdminAccessGate allow={canManage}>
	{#if mouseState.loading}
		<div in:fade>
			<LoadingCard title={m.airmouse_title()} icon={Mouse} loading={true} />
		</div>
	{:else if mouseState.error}
		<div class="alert alert-error mb-4" in:fade>
			<span>{mouseState.error}</span>
		</div>
	{:else}
		<AirMouseMain {mouseState} />
	{/if}
</AdminAccessGate>
