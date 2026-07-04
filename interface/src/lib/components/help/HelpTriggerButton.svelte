<script lang="ts">
	import { FormButton } from '$lib/components/shared/forms';
	import HelpCircle from '~icons/tabler/help-circle';

	interface Props {
		label: string;
		iconOnly?: boolean;
		class?: string;
		onclick?: (_event: MouseEvent) => void;
	}

	let { label, iconOnly = true, class: className = '', onclick = () => {} }: Props = $props();
</script>

{#if iconOnly}
	<FormButton
		variant="icon"
		size="sm"
		icon={HelpCircle}
		ariaLabel={label}
		title={label}
		{onclick}
		class={className}
	/>
{:else}
	<FormButton
		variant="ghost"
		size="sm"
		icon={HelpCircle}
		{label}
		ariaLabel={label}
		title={label}
		{onclick}
		class={className}
	/>
{/if}
