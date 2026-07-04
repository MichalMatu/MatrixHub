<script lang="ts">
	import { AdminAccessGate } from '$lib/components/layout';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import { useAirMouseManagement } from '$lib/components/airmouse/airMouseState.svelte';
	import AirMouseJiggler from '$lib/components/airmouse/components/AirMouseJiggler.svelte';
	import { fade } from 'svelte/transition';
	import Activity from '~icons/tabler/activity';
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
			<LoadingCard title={m.jiggler_title()} icon={Activity} loading={true} />
		</div>
	{:else}
		{#if mouseState.error}
			<div class="alert alert-error mb-4" in:fade>
				<span>{mouseState.error}</span>
			</div>
		{/if}

		<div class="grid grid-cols-1 md:grid-cols-2 gap-4">
			<AirMouseJiggler {mouseState} />
		</div>
	{/if}
</AdminAccessGate>
