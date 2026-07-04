<script lang="ts">
	import CompensationSettings from './CompensationSettings.svelte';
	import CompensationPreview from './CompensationPreview.svelte';
	import { useCompensationSettings } from './useCompensationSettings.svelte';
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const compState = useCompensationSettings({ shouldLoad: () => canManage });
	const showPreviewCard = $derived(compState.canPreview && !compState.loading);
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<CompensationSettings {compState} />
		{#if showPreviewCard}
			<CompensationPreview {compState} />
		{/if}
	</GridLayout>
</AdminAccessGate>
