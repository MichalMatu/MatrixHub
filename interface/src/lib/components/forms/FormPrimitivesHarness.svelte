<script lang="ts">
	import { FormInput, FormSelect } from '$lib/components/shared/forms';

	let ssid = $state('MatrixHub');
	let password = $state('secret');
	let mode = $state('b');
</script>

<div class="space-y-4">
	<FormInput
		label="SSID"
		data-testid="primitives-ssid"
		bind:value={ssid}
		maxlength={32}
		error="SSID too long (max 32 bytes)"
	/>

	<FormInput
		label="Password"
		type="password"
		data-testid="primitives-password"
		help="Hint text"
		bind:value={password}
	>
		{#snippet suffix()}
			<span data-testid="suffix-addon" class="text-xs text-base-content/60">ms</span>
		{/snippet}
	</FormInput>

	<FormSelect
		label="Mode"
		data-testid="mode-select"
		bind:value={mode}
		size="sm"
		options={[
			{ value: 'a', label: 'Mode A' },
			{ value: 'b', label: 'Mode B' }
		]}
	/>
</div>
