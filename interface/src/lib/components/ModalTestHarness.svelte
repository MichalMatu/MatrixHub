<script lang="ts">
	import Modal from './Modal.svelte';

	interface Props {
		isOpen?: boolean;
		closeOnOutsideClick?: boolean;
		onClose: () => void;
	}

	let { isOpen = true, closeOnOutsideClick = true, onClose }: Props = $props();
</script>

<Modal {isOpen} {onClose} title="Test modal" {closeOnOutsideClick}>
	<p>Modal body</p>

	{#snippet actions()}
		<button type="button">Action</button>
	{/snippet}
</Modal>
