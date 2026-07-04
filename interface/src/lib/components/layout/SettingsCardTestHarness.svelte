<script lang="ts">
	import SettingsCard from './SettingsCard.svelte';

	let {
		hasChanges = false,
		loading = false,
		saving = false,
		disabled = false,
		error = undefined,
		dirtySourceId = undefined,
		onSave = () => {},
		onReset = () => {}
	}: {
		hasChanges?: boolean;
		loading?: boolean;
		saving?: boolean;
		disabled?: boolean;
		error?: string;
		dirtySourceId?: string;
		onSave?: () => void | Promise<unknown>;
		onReset?: () => void;
	} = $props();
</script>

<SettingsCard
	title="Settings"
	{hasChanges}
	{loading}
	{saving}
	{disabled}
	{error}
	{dirtySourceId}
	{onSave}
	{onReset}
>
	<div>Settings body</div>
</SettingsCard>
