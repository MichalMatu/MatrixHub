<script lang="ts">
	import Admin from '~icons/tabler/shield-check'; // Changing Admin icon to shield for clarity vs Key
	import Delete from '~icons/tabler/trash';
	import Edit from '~icons/tabler/pencil';
	import type { UserSetting } from '$lib/services/api/core/SecurityApiService';
	import { FormButton } from '$lib/components/shared/forms';
	import StatusRow from '$lib/components/layout/StatusRow.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		userItem: UserSetting;
		onEdit: () => void;
		onDelete: () => void;
	}

	let { userItem, onEdit, onDelete }: Props = $props();
</script>

<StatusRow
	icon={Edit}
	iconClass="h-6 w-6 flex-none text-base-content/70"
	label={userItem.username}
	labelClass="font-bold truncate"
>
	{#snippet details()}
		{#if userItem.admin}
			<div
				class="flex items-center gap-1 text-xs text-secondary font-medium uppercase tracking-wider"
			>
				<Admin class="h-3 w-3" />
				{m.role_admin({ locale: i18n.languageTag })}
			</div>
		{:else}
			<div class="text-xs opacity-50 uppercase tracking-wider font-medium">
				{m.role_user({ locale: i18n.languageTag })}
			</div>
		{/if}
	{/snippet}
	{#snippet actions()}
		<FormButton
			label=""
			icon={Edit}
			onclick={onEdit}
			class="btn-ghost btn-circle btn-sm"
			ariaLabel={m.action_edit({ locale: i18n.languageTag })}
			title={m.action_edit({ locale: i18n.languageTag })}
		/>
		<FormButton
			label=""
			icon={Delete}
			onclick={onDelete}
			class="btn-ghost btn-circle btn-sm text-error"
			ariaLabel={m.action_delete({ locale: i18n.languageTag })}
			title={m.action_delete({ locale: i18n.languageTag })}
		/>
	{/snippet}
</StatusRow>
