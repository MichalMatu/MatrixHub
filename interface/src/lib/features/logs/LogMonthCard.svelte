<script lang="ts">
	import Calendar from '~icons/tabler/calendar';
	import Download from '~icons/tabler/download';
	import Trash from '~icons/tabler/trash';
	import type { LogMonth } from '$lib/services/api/monitoring/LogsApiService';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	let {
		month,
		canDelete = true,
		onDownload,
		onDelete
	}: {
		month: LogMonth;
		canDelete?: boolean;
		onDownload: (_path: string, _name: string) => void;
		onDelete: (_path: string, _name: string) => void;
	} = $props();
</script>

<BaseCard title={month.name} icon={Calendar}>
	<div class="flex flex-col gap-1">
		{#each month.files as file (file.name)}
			<ContentBox
				class="flex items-center justify-between px-4 py-2 hover:bg-base-100/80 transition-colors"
			>
				<div class="flex flex-col min-w-0">
					<div class="font-bold text-sm truncate">{file.name}</div>
					<div class="text-xs opacity-50">{(file.size / 1024).toFixed(1)} KB</div>
				</div>

				<div class="flex items-center gap-1">
					<FormButton
						label=""
						icon={Download}
						class="btn-ghost btn-sm btn-square text-primary"
						onclick={() => onDownload(month.path, file.name)}
						title={m.action_download({ locale: i18n.languageTag })}
					/>
					{#if canDelete}
						<FormButton
							label=""
							icon={Trash}
							class="btn-ghost btn-sm btn-square text-error"
							onclick={() => onDelete(month.path, file.name)}
							title={m.action_delete({ locale: i18n.languageTag })}
						/>
					{/if}
				</div>
			</ContentBox>
		{/each}
	</div>
</BaseCard>
