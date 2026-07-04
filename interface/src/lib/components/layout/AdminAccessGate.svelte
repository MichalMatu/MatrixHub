<script lang="ts">
	import AdminAccessCard from './AdminAccessCard.svelte';
	import PageWrapper from './PageWrapper.svelte';

	interface Props {
		allow: boolean;
		title?: string;
		message?: string;
		children?: import('svelte').Snippet;
	}

	let { allow, title, message, children }: Props = $props();
</script>

<PageWrapper>
	{#if allow}
		{@render children?.()}
	{:else}
		<AdminAccessCard {title} {message} />
	{/if}
</PageWrapper>
