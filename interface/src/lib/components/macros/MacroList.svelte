<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import type { MacroApiService, ScriptFile } from '$lib/services/api/integrations/MacroApiService';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import DataTable from '$lib/components/shared/tables/DataTable.svelte';
	import { confirm } from '$lib/utils/ui/dialogs';

	// Icons
	import IconPlayerPlay from '~icons/tabler/player-play';
	import IconPlayerStop from '~icons/tabler/player-stop';
	import IconEdit from '~icons/tabler/edit';
	import IconTrash from '~icons/tabler/trash';

	import * as m from '$lib/paraglide/messages.js';

	import { useMacroStatusSync } from '$lib/features/system/status/useMacroStatusSync.svelte';

	type MacroApiLike = Pick<
		MacroApiService,
		'deleteScript' | 'getStatus' | 'runScript' | 'stopScript'
	>;

	let {
		api,
		scripts = [],
		loading = false,
		onedit,
		enabled = true,
		onrun,
		onstop,
		ondelete
	}: {
		api: MacroApiLike;
		scripts?: ScriptFile[];
		loading?: boolean;
		onedit?: (name: string) => void;
		enabled?: boolean;
		onrun?: (name: string) => Promise<void>;
		onstop?: () => Promise<void>;
		ondelete?: (name: string) => Promise<void>;
	} = $props();

	const macroStatus = useMacroStatusSync({
		createApi: () => api
	});
	let status = $derived(macroStatus.status);

	onMount(async () => {
		await macroStatus.init();
	});

	onDestroy(() => {
		macroStatus.destroy();
	});

	async function onRun(name: string) {
		if (!enabled) return;
		try {
			if (onrun) {
				await onrun(name);
			} else {
				await api.runScript(name);
			}
		} catch (e) {
			console.error(e);
		} finally {
			await macroStatus.refreshStatus();
		}
	}

	async function onStop() {
		if (!enabled) return;
		try {
			if (onstop) {
				await onstop();
			} else {
				await api.stopScript();
			}
		} catch (e) {
			console.error(e);
		} finally {
			await macroStatus.refreshStatus();
		}
	}

	function onEdit(name: string) {
		onedit?.(name);
	}

	function onDeleteClick(name: string) {
		confirm({
			title: m.macros_delete_title(),
			message: m.macros_delete_msg({ name }),
			onConfirm: () => doDelete(name)
		});
	}

	async function doDelete(name: string) {
		try {
			if (ondelete) {
				await ondelete(name);
			} else {
				await api.deleteScript(name);
			}
		} catch (e) {
			console.error(e);
		}
	}

	function getStatusLabel(s: string): string {
		switch (s) {
			case 'IDLE':
				return m.macros_status_idle();
			case 'RUNNING':
				return m.macros_status_running();
			case 'PAUSED':
				return m.macros_status_paused();
			case 'ERROR':
				return m.macros_status_error();
			case 'COMPLETED':
				return m.macros_status_completed();
			default:
				return s;
		}
	}
</script>

<div class="flex flex-col gap-1">
	<!-- Status Panel -->
	<div class="grid grid-cols-1 md:grid-cols-3 gap-1">
		<ContentBox class="flex flex-col justify-between">
			<div class="text-sm opacity-70 mb-1">{m.macros_status()}</div>
			<div class="text-xl font-bold" class:text-primary={status?.status === 'RUNNING'}>
				{status ? getStatusLabel(status.status) : m.macros_status_idle()}
			</div>
			<div class="text-xs opacity-50 truncate">
				{status?.current_script || m.macros_no_script()}
			</div>
			{#if status?.status === 'ERROR' && status?.last_error}
				<div class="text-xs text-error truncate">{status.last_error}</div>
			{/if}
		</ContentBox>

		<ContentBox class="flex flex-col justify-between">
			<div class="text-sm opacity-70 mb-1">{m.macros_line()}</div>
			<div class="text-xl font-bold">{status?.current_line || 0}</div>
		</ContentBox>

		<ContentBox class="flex items-center justify-center">
			<FormButton
				label={m.macros_stop()}
				icon={IconPlayerStop}
				variant="danger"
				class="w-full h-full"
				disabled={!enabled || status?.status !== 'RUNNING'}
				onclick={onStop}
			/>
		</ContentBox>
	</div>

	<!-- Script List -->
	<div class="overflow-x-auto bg-base-100 rounded-box border border-base-300">
		<DataTable size="">
			<thead>
				<tr>
					<th>{m.macros_script_name()}</th>
					<th class="text-right">{m.macros_actions()}</th>
				</tr>
			</thead>
			<tbody>
				{#if loading && scripts.length === 0}
					<tr><td colspan="2" class="text-center">{m.macros_loading()}</td></tr>
				{:else if scripts.length === 0}
					<tr>
						<td colspan="2" class="text-center text-base-content/50">
							{m.macros_no_scripts()}
						</td>
					</tr>
				{:else}
					{#each scripts as script}
						<tr class="hover">
							<td class="font-mono">{script.name}</td>
							<td class="text-right">
								<div class="flex flex-wrap items-center justify-end gap-1">
									<FormButton
										label=""
										icon={IconPlayerPlay}
										variant="ghost"
										size="sm"
										onclick={() => onRun(script.name)}
										title={m.macros_action_run()}
										ariaLabel={m.macros_action_run()}
										disabled={!enabled ||
											(status?.status === 'RUNNING' && status?.current_script === script.name)}
									/>
									<FormButton
										label=""
										icon={IconEdit}
										variant="ghost"
										size="sm"
										onclick={() => onEdit(script.name)}
										title={m.macros_action_edit()}
										ariaLabel={m.macros_action_edit()}
									/>
									<FormButton
										label=""
										icon={IconTrash}
										variant="ghost"
										size="sm"
										class="text-error"
										onclick={() => onDeleteClick(script.name)}
										title={m.macros_action_delete()}
										ariaLabel={m.macros_action_delete()}
									/>
								</div>
							</td>
						</tr>
					{/each}
				{/if}
			</tbody>
		</DataTable>
	</div>
</div>
