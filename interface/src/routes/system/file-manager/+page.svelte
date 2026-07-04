<!-- @runes -->
<script lang="ts">
	import { formatBytes, createApiClient } from '$lib/utils';
	import { confirm } from '$lib/utils/ui/dialogs';
	import FeatureHelpModal from '$lib/components/help/FeatureHelpModal.svelte';
	import HelpTriggerButton from '$lib/components/help/HelpTriggerButton.svelte';
	import { getBreadcrumbs } from '$lib/utils/path/helpers';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { createFileManagerState } from '$lib/stores/fileManagerState';
	import { isFileManagerUploadBlockedPath, type FileEntry } from '$lib/services/fileManager';
	import { AdminAccessCard, PageWrapper } from '$lib/components/layout';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormButton, FormFileInput } from '$lib/components/shared/forms';
	import FileManagerDirectoryList from '$lib/components/system/file-manager/FileManagerDirectoryList.svelte';
	import FileManagerToolbar from '$lib/components/system/file-manager/FileManagerToolbar.svelte';
	import FolderIcon from '~icons/tabler/folder';
	import AlertIcon from '~icons/tabler/alert-triangle';
	import TrashIcon from '~icons/tabler/trash';
	import CancelIcon from '~icons/tabler/x';
	import UploadIcon from '~icons/tabler/upload';
	import { appFeatures } from '$lib/stores/appFeatures.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	const session = useSessionAccess();
	const apiClient = createApiClient(session.apiOptions);

	const fileManager = createFileManagerState({ apiClient });
	const {
		state: fmState,
		loadDirectory,
		openEntry,
		downloadEntry,
		deleteEntry,
		handleUpload,
		refresh,
		goToParent,
		setUploadFiles,
		formatEntrySize,
		getEntryLabel,
		canDeleteEntry
	} = fileManager;

	const canModify = $derived(session.canManage);
	const features = $derived(appFeatures.features);
	const featuresResolved = $derived(appFeatures.resolved);
	const featureEnabled = $derived(featuresResolved ? (features.file_manager ?? true) : false);
	const isCurrentPathUploadBlocked = $derived(isFileManagerUploadBlockedPath(fmState.currentPath));
	let initialRefreshStarted = $state(false);
	let helpOpen = $state(false);
	const locale = $derived(i18n.languageTag);
	const storageSummary = $derived.by(() => {
		const info = fmState.systemState.systemInformation;
		if (!info) {
			return null;
		}

		const totalBytes = info.fs_total || 0;
		const usedBytes = info.fs_used || 0;

		return {
			totalBytes,
			usedBytes,
			freeBytes: totalBytes - usedBytes,
			usedPercent: totalBytes > 0 ? (usedBytes / totalBytes) * 100 : 0
		};
	});

	$effect(() => {
		if (initialRefreshStarted || !featuresResolved || !featureEnabled || !canModify) {
			return;
		}

		initialRefreshStarted = true;
		void refresh();
	});

	function requestDelete(entry: FileEntry) {
		confirm({
			title: 'Remove Entry',
			message: `Are you sure you want to remove "${getEntryLabel(entry)}"?`,
			labels: {
				cancel: { label: 'Cancel', icon: CancelIcon },
				confirm: { label: 'Delete', icon: TrashIcon }
			},
			onConfirm: () => {
				void deleteEntry(entry);
			}
		});
	}

	const helpSections = $derived([
		{
			title: m.help_modal_how_title({ locale }),
			body: m.file_manager_help_how_body({ locale })
		},
		{
			title: m.help_modal_setup_title({ locale }),
			body: m.file_manager_help_setup_body({ locale })
		},
		{
			title: m.help_modal_watch_title({ locale }),
			body: m.file_manager_help_watch_body({ locale })
		}
	]);
	const helpLinks = $derived([
		{ href: '/system/status', label: m.menu_status({ locale }) },
		{ href: '/logs', label: m.menu_logs({ locale }) },
		{ href: '/system/help', label: m.menu_help({ locale }) }
	]);
</script>

<PageWrapper>
	{#if !featuresResolved}
		<BaseCard title="File Manager" icon={FolderIcon}>
			<div class="flex items-center justify-center py-10 text-base-content/70">
				<span>{m.common_loading({ locale: i18n.languageTag })}</span>
			</div>
		</BaseCard>
	{:else if !featureEnabled}
		<BaseCard title="File Manager" icon={FolderIcon}>
			<div class="space-y-6">
				<div class="flex flex-col gap-5 sm:flex-row sm:items-center sm:justify-between">
					<div class="flex items-center gap-4">
						<div class="rounded-2xl bg-warning/10 p-3 text-warning">
							<AlertIcon class="h-7 w-7" />
						</div>
						<div class="space-y-1">
							<p class="text-xs uppercase tracking-wide text-base-content/60">File Access</p>
							<h1 class="text-2xl font-semibold text-base-content">File Manager</h1>
							<p class="text-sm text-base-content/70">
								This device has disabled remote file browsing. Enable the feature in firmware
								settings to manage storage from the dashboard.
							</p>
						</div>
					</div>
				</div>

				<ContentBox paddingClass="px-4 py-3" class="border-warning/40 bg-warning/10 text-warning">
					<div class="flex items-start gap-3">
						<AlertIcon class="mt-0.5 h-5 w-5" />
						<span>The file manager feature is disabled on this device.</span>
					</div>
				</ContentBox>
			</div>
		</BaseCard>
	{:else if canModify}
		<div class="grid grid-cols-1 gap-6 lg:grid-cols-3">
			<div class="lg:col-span-2">
				<BaseCard title="File Manager" icon={FolderIcon} class="h-full">
					{#snippet actions()}
						<div class="flex items-center gap-2">
							<HelpTriggerButton
								label={m.file_manager_help_title({ locale })}
								onclick={() => (helpOpen = true)}
							/>
							<span
								class="badge badge-outline border-secondary/40 bg-secondary/10 text-xs uppercase tracking-wide text-secondary"
							>
								Admin
							</span>
						</div>
					{/snippet}

					<div class="space-y-5">
						<FileManagerToolbar
							breadcrumbs={getBreadcrumbs(fmState.currentPath)}
							isLoading={fmState.isLoading}
							canGoParent={fmState.currentPath !== '/'}
							backendLabel="LittleFS"
							onNavigateRoot={() => void loadDirectory('/')}
							onSelectBreadcrumb={(path: string) => void loadDirectory(path)}
							onGoParent={() => void goToParent()}
							onRefresh={() => void refresh()}
						/>

						<FileManagerDirectoryList
							entries={fmState.entries}
							isLoading={fmState.isLoading}
							errorMessage={fmState.errorMessage}
							{formatEntrySize}
							{getEntryLabel}
							onOpenEntry={(entry: FileEntry) => void openEntry(entry)}
							onDownloadEntry={(entry: FileEntry) => void downloadEntry(entry)}
							onRequestDelete={requestDelete}
							canDelete={(entry: FileEntry) => canDeleteEntry(entry, canModify)}
						/>
					</div>
				</BaseCard>
			</div>

			<div class="space-y-6 lg:col-span-1">
				<BaseCard title="Upload Files" icon={UploadIcon}>
					<div class="space-y-4">
						{#if isCurrentPathUploadBlocked}
							<ContentBox
								paddingClass="px-4 py-3"
								class="border-warning/40 bg-warning/10 text-warning"
							>
								<div class="flex items-start gap-3">
									<AlertIcon class="mt-0.5 h-5 w-5" />
									<span>
										{m.file_manager_upload_blocked_path({ locale: i18n.languageTag })}
									</span>
								</div>
							</ContentBox>
						{/if}

						<FormFileInput
							label="Select files"
							files={fmState.uploadFiles}
							onchange={(event: Event) => {
								const target = event.target as HTMLInputElement;
								setUploadFiles(target.files);
							}}
							disabled={fmState.uploading || isCurrentPathUploadBlocked}
							size="sm"
						/>

						<FormButton
							variant="primary"
							size="sm"
							class="w-full"
							onclick={() => void handleUpload()}
							disabled={!fmState.uploadFiles?.length ||
								fmState.uploading ||
								isCurrentPathUploadBlocked}
							loading={fmState.uploading}
						>
							<UploadIcon class="mr-2 h-4 w-4" />
							Upload
						</FormButton>
					</div>
				</BaseCard>

				<BaseCard title="Storage Overview">
					{#if storageSummary}
						<div class="space-y-3">
							<div class="flex items-end justify-between">
								<span class="text-sm font-medium">Internal Partition</span>
								<span class="text-xs opacity-60">
									{formatBytes(storageSummary.usedBytes)} / {formatBytes(storageSummary.totalBytes)}
								</span>
							</div>

							<div class="h-2 w-full overflow-hidden rounded-full bg-base-200">
								<div
									class="h-full bg-primary transition-all duration-500"
									style="width: {storageSummary.usedPercent}%"
								></div>
							</div>

							<div class="flex items-center justify-between text-xs">
								<span class="opacity-60">Available Space</span>
								<span class="font-bold text-primary">{formatBytes(storageSummary.freeBytes)}</span>
							</div>
						</div>
					{:else}
						<div class="flex justify-center p-4">
							<span class="loading loading-spinner loading-sm opacity-30"></span>
						</div>
					{/if}
				</BaseCard>
			</div>
		</div>
	{:else}
		<AdminAccessCard />
	{/if}

	<FeatureHelpModal
		isOpen={helpOpen}
		onClose={() => (helpOpen = false)}
		title={m.file_manager_help_title({ locale })}
		intro={m.file_manager_help_intro({ locale })}
		sections={helpSections}
		links={helpLinks}
	/>
</PageWrapper>
