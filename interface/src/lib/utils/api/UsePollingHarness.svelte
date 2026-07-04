<svelte:options runes={false} />

<script lang="ts">
	import { usePolling, type PollingOptions } from './usePolling.svelte';

	interface PollingHarnessController {
		start: () => void;
		stop: () => void;
		poll: () => Promise<void>;
		isPolling: () => boolean;
		isFetching: () => boolean;
	}

	export let fetchFn: () => Promise<void>;
	export let options: PollingOptions = {};
	export let onReady: (controller: PollingHarnessController) => void = () => {};

	const polling = usePolling(fetchFn, options);

	$: {
		onReady({
			start: polling.start,
			stop: polling.stop,
			poll: polling.poll,
			isPolling: () => polling.isPolling,
			isFetching: () => polling.isFetching
		});
	}
</script>

<div data-testid="polling-harness"></div>
