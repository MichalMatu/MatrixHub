<script lang="ts">
	import Loader from '~icons/tabler/loader-2';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let { text = undefined } = $props();
	const fallbackText = $derived(m.common_loading({ locale: i18n.languageTag }));
</script>

<div class="flex w-full flex-col items-center justify-center p-4">
	<Loader class="text-primary h-10 w-auto animate-spin stroke-2 mb-2" />
	<p class="text-base font-medium opacity-70">{text ?? fallbackText}</p>
</div>
