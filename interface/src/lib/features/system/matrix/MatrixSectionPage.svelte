<script lang="ts">
	import { untrack, type Snippet } from 'svelte';
	import { MatrixApiService } from '$lib/services/api/core/MatrixApiService';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { PageWrapper, GridLayout } from '$lib/components/layout';
	import MatrixAccessNotice from './MatrixAccessNotice.svelte';
	import MatrixSectionTabs from './MatrixSectionTabs.svelte';
	import { useMatrixSettings } from './useMatrixSettings.svelte';
	import type { MatrixSettingsKey } from './matrixModel';

	type MatrixSettingsStore = ReturnType<typeof useMatrixSettings>;

	let {
		trackedKeys,
		children
	}: {
		trackedKeys: readonly MatrixSettingsKey[];
		children: Snippet<[MatrixSettingsStore, boolean]>;
	} = $props();

	const session = useSessionAccess();
	const canRead = $derived(session.canRead);
	const canManage = $derived(session.canManage);
	const matrixState = useMatrixSettings(() => new MatrixApiService(session.apiOptions), {
		shouldLoad: () => canRead,
		trackedKeys: untrack(() => trackedKeys)
	});
</script>

<PageWrapper>
	<MatrixSectionTabs />
	<GridLayout cols={1}>
		{#if !canRead}
			<MatrixAccessNotice />
		{:else}
			{@render children(matrixState, canManage)}
		{/if}
	</GridLayout>
</PageWrapper>
