<!-- @runes -->
<script lang="ts">
	import Spinner from '$lib/components/common/Spinner.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import DataTable from '$lib/components/shared/tables/DataTable.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import type { FileEntry } from '$lib/services/fileManager';
	import AlertIcon from '~icons/tabler/alert-triangle';
	import DownloadIcon from '~icons/tabler/download';
	import FileIcon from '~icons/tabler/file';
	import FolderIcon from '~icons/tabler/folder';
	import InfoIcon from '~icons/tabler/info-circle';
	import TrashIcon from '~icons/tabler/trash';

	type EntryFormatter = (entry: FileEntry) => string;
	type EntryHandler = (entry: FileEntry) => void;
	type EntryPredicate = (entry: FileEntry) => boolean;

	interface Props {
		entries?: FileEntry[];
		isLoading?: boolean;
		errorMessage?: string | null;
		formatEntrySize?: EntryFormatter;
		getEntryLabel?: EntryFormatter;
		onOpenEntry?: EntryHandler;
		onDownloadEntry?: EntryHandler;
		onRequestDelete?: EntryHandler;
		canDelete?: EntryPredicate;
	}

	let {
		entries = [],
		isLoading = false,
		errorMessage = null,
		formatEntrySize = (entry: FileEntry): string => {
			void entry;
			return '—';
		},
		getEntryLabel = (entry: FileEntry): string => entry.name,
		onOpenEntry = (entry: FileEntry): void => {
			void entry;
		},
		onDownloadEntry = (entry: FileEntry): void => {
			void entry;
		},
		onRequestDelete = (entry: FileEntry): void => {
			void entry;
		},
		canDelete = (entry: FileEntry): boolean => {
			void entry;
			return false;
		}
	}: Props = $props();
</script>

<div class="space-y-5">
	{#if errorMessage}
		<ContentBox paddingClass="px-4 py-3" class="border-error/40 bg-error/10 text-error">
			<div class="flex items-start gap-3">
				<AlertIcon class="mt-0.5 h-5 w-5" />
				<span>{errorMessage}</span>
			</div>
		</ContentBox>
	{/if}

	{#if isLoading}
		<div class="flex justify-center py-12">
			<Spinner />
		</div>
	{:else if entries.length === 0}
		<ContentBox paddingClass="px-4 py-3" class="border-info/40 bg-info/10 text-info">
			<div class="flex items-start gap-3">
				<InfoIcon class="mt-0.5 h-5 w-5" />
				<span>{m.file_manager_empty_directory({ locale: i18n.languageTag })}</span>
			</div>
		</ContentBox>
	{:else}
		<ContentBox paddingClass="p-0" class="overflow-hidden">
			<DataTable>
				<thead>
					<tr class="text-xs uppercase tracking-wide text-base-content/60">
						<th class="px-4 py-3 text-left"
							>{m.file_manager_col_name({ locale: i18n.languageTag })}</th
						>
						<th class="px-4 py-3 text-left"
							>{m.file_manager_col_size({ locale: i18n.languageTag })}</th
						>
						<th class="px-4 py-3 text-right"
							>{m.file_manager_col_actions({ locale: i18n.languageTag })}</th
						>
					</tr>
				</thead>
				<tbody>
					{#each entries as entry (entry.name)}
						<tr class="border-t border-base-200/40 text-sm hover:bg-base-100">
							<td class="px-4 py-3">
								<button
									type="button"
									class="flex w-full items-center gap-2 text-left"
									onclick={() => onOpenEntry(entry)}
								>
									{#if entry.type === 'directory'}
										<FolderIcon class="h-5 w-5 text-primary" />
									{:else if entry.type === 'file'}
										<FileIcon class="h-5 w-5 text-secondary" />
									{:else}
										<FileIcon class="h-5 w-5 text-base-content/60" />
									{/if}
									<span class="font-medium text-base-content">{getEntryLabel(entry)}</span>
								</button>
							</td>
							<td class="px-4 py-3 text-base-content/70">{formatEntrySize(entry)}</td>
							<td class="px-4 py-3 text-right">
								<div class="inline-flex items-center gap-2">
									{#if entry.type === 'directory'}
										<FormButton
											variant="icon"
											size="xs"
											class="h-8 w-8"
											ariaLabel={m.file_manager_open_directory({ locale: i18n.languageTag })}
											title={m.file_manager_open_directory({ locale: i18n.languageTag })}
											onclick={() => onOpenEntry(entry)}
										>
											<FolderIcon class="h-4 w-4" />
										</FormButton>
									{:else}
										<FormButton
											variant="icon"
											size="xs"
											class="h-8 w-8"
											ariaLabel={m.file_manager_download_file({ locale: i18n.languageTag })}
											title={m.file_manager_download_file({ locale: i18n.languageTag })}
											onclick={() => onDownloadEntry(entry)}
										>
											<DownloadIcon class="h-4 w-4" />
										</FormButton>
									{/if}
									{#if canDelete(entry)}
										<FormButton
											variant="icon"
											size="xs"
											class="h-8 w-8 text-error"
											ariaLabel={m.file_manager_remove_entry({ locale: i18n.languageTag })}
											title={m.file_manager_remove_entry({ locale: i18n.languageTag })}
											onclick={() => onRequestDelete(entry)}
										>
											<TrashIcon class="h-4 w-4" />
										</FormButton>
									{/if}
								</div>
							</td>
						</tr>
					{/each}
				</tbody>
			</DataTable>
		</ContentBox>
	{/if}
</div>
