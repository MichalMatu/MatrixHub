<script lang="ts">
	import SystemStatus from '$lib/features/system/status/SystemStatus.svelte';
	import LiveTail from '$lib/features/system/status/LiveTail.svelte';
	import { PageWrapper } from '$lib/components/layout';
</script>

<PageWrapper>
	<!-- Integrated System Status with Health Diagnostics -->
	<SystemStatus />

	<div class="mt-3 md:mt-4">
		<LiveTail />
	</div>
</PageWrapper>
