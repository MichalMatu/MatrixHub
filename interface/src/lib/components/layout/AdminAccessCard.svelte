<script lang="ts">
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';
	import LockAccess from '~icons/tabler/lock-access';
	import BaseCard from './BaseCard.svelte';

	let {
		title = m.access_admin_required_title({ locale: i18n.languageTag }),
		message = m.access_admin_required_desc({ locale: i18n.languageTag })
	}: {
		title?: string;
		message?: string;
	} = $props();
</script>

<BaseCard
	{title}
	icon={LockAccess}
	iconClass="h-6 w-6 text-warning flex-none"
	class="mx-auto max-w-2xl"
>
	<p class="text-sm leading-6 text-base-content/80">{message}</p>
</BaseCard>
