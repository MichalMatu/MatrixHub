<script lang="ts">
	// Checkbox shares the same primitive layer as input/select/toggle so form building
	// blocks are imported consistently from $lib/components.
	let {
		checked = $bindable(false),
		label,
		disabled = false,
		class: className = '',
		onchange
	}: {
		checked?: boolean;
		label: string;
		disabled?: boolean;
		class?: string;
		onchange?: (_: Event) => void;
	} = $props();
</script>

<label class={`label cursor-pointer gap-2 justify-start p-0 ${className}`}>
	<input
		type="checkbox"
		bind:checked
		class="checkbox checkbox-primary checkbox-sm"
		{disabled}
		{onchange}
	/>
	<span class="text-sm font-bold">{label}</span>
</label>
