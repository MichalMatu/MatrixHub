<script lang="ts">
	import { MediaQuery } from 'svelte/reactivity';
	import CompensationSettings from './CompensationSettings.svelte';
	import CompensationPreview from './CompensationPreview.svelte';
	import { useCompensationSettings } from './useCompensationSettings.svelte';
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const compState = useCompensationSettings({ shouldLoad: () => canManage });
	const isStackedLayout = new MediaQuery('max-width: 767px', false);
	const showPreviewCard = $derived(compState.canPreview && !compState.loading);
	const showSaveInSettings = $derived(isStackedLayout.current || !showPreviewCard);
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<CompensationSettings {compState} showSaveButton={showSaveInSettings} />
		{#if showPreviewCard}
			<CompensationPreview {compState} />
		{/if}
	</GridLayout>
</AdminAccessGate>
