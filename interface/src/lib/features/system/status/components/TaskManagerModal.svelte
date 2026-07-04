<script lang="ts">
	import { Spinner } from '$lib/components';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { useTaskManager } from '../useTaskManager.svelte';
	import DataTable from '$lib/components/shared/tables/DataTable.svelte';

	import Refresh from '~icons/tabler/refresh';
	import Activity from '~icons/tabler/activity';
	import X from '~icons/tabler/x';
	import { Modal } from '$lib/components';
	import { FormButton } from '$lib/components/shared/forms';

	let { open = $bindable(false) } = $props();

	const taskManager = useTaskManager({
		isOpen: () => open
	});

	function getPriorityClass(p: number) {
		if (p > 20) return 'text-error font-bold';
		if (p > 10) return 'text-warning font-bold';
		return 'text-success';
	}

	function getStateBadge(state: string) {
		switch (state) {
			case 'running':
				return 'badge-success';
			case 'ready':
				return 'badge-info';
			case 'blocked':
				return 'badge-warning';
			case 'suspended':
				return 'badge-neutral';
			case 'deleted':
				return 'badge-error';
			default:
				return 'badge-ghost';
		}
	}

	function getStateLabel(state: string) {
		switch (state) {
			case 'running':
				return m.task_manager_state_running({ locale: i18n.languageTag });
			case 'ready':
				return m.task_manager_state_ready({ locale: i18n.languageTag });
			case 'blocked':
				return m.task_manager_state_blocked({ locale: i18n.languageTag });
			case 'suspended':
				return m.task_manager_state_suspended({ locale: i18n.languageTag });
			case 'deleted':
				return m.task_manager_state_deleted({ locale: i18n.languageTag });
			default:
				return state;
		}
	}

	function getStackColorClass(bytes: number) {
		if (bytes > 2048) return 'text-success opacity-80';
		if (bytes > 512) return 'text-warning font-bold';
		return 'text-error font-bold';
	}
</script>

<Modal isOpen={open} onClose={() => (open = false)} widthClass="w-full max-w-2xl">
	<!-- Custom Title for Task Manager -->
	<div class="mb-2 flex items-center justify-between">
		<h3 class="flex items-center gap-2 text-lg font-bold">
			<Activity class="h-5 w-5 text-primary" />
			{m.task_manager_title({ locale: i18n.languageTag })}
		</h3>
		<div class="flex gap-1">
			<FormButton
				label=""
				icon={Refresh}
				class="btn-ghost btn-xs"
				onclick={taskManager.refresh}
				disabled={taskManager.loading}
				loading={taskManager.loading}
				ariaLabel={m.action_refresh_status({ locale: i18n.languageTag })}
				title={m.action_refresh_status({ locale: i18n.languageTag })}
			/>
			<FormButton
				label=""
				icon={X}
				class="btn-ghost btn-xs"
				onclick={() => (open = false)}
				ariaLabel={m.action_close({ locale: i18n.languageTag })}
				title={m.action_close({ locale: i18n.languageTag })}
			/>
		</div>
	</div>

	{#if taskManager.loading && !taskManager.tasksData}
		<div class="flex justify-center items-center py-8">
			<Spinner />
		</div>
	{:else if taskManager.error}
		<div class="alert alert-error">
			<span>{m.error_prefix({ error: taskManager.error }, { locale: i18n.languageTag })}</span>
		</div>
	{:else if taskManager.tasksData}
		<!-- Compact Stats Bar -->
		<div class="mb-4 grid grid-cols-2 gap-2 sm:grid-cols-5 text-center">
			<div class="rounded-box bg-base-200 py-2 px-1 flex flex-col items-center justify-center">
				<div class="text-[10px] font-bold opacity-60 uppercase tracking-wider">
					{m.task_manager_tasks({ locale: i18n.languageTag })}
				</div>
				<div class="text-lg font-bold text-primary leading-tight">
					{taskManager.tasksData.taskCount}
				</div>
			</div>

			<div class="rounded-box bg-base-200 py-2 px-1 flex flex-col items-center justify-center">
				<div class="text-[10px] font-bold opacity-60 uppercase tracking-wider">
					{m.task_manager_watchdog({ locale: i18n.languageTag })}
				</div>
				<div
					class="text-lg font-bold leading-tight"
					class:text-success={taskManager.tasksData.watchdog.initialized}
				>
					{taskManager.tasksData.watchdog.initialized
						? m.task_manager_status_active({ locale: i18n.languageTag })
						: m.task_manager_status_inactive({ locale: i18n.languageTag })}
				</div>
				<div class="text-[9px] opacity-70 leading-none mt-0.5">
					{m.task_manager_timeout(
						{ timeout: taskManager.tasksData.watchdog.timeoutSec },
						{ locale: i18n.languageTag }
					)}
				</div>
			</div>

			<div class="rounded-box bg-base-200 py-2 px-1 flex flex-col items-center justify-center">
				<div class="text-[10px] font-bold opacity-60 uppercase tracking-wider">
					{m.task_manager_dram({ locale: i18n.languageTag })}
				</div>
				<div class="text-lg font-bold text-secondary leading-tight">
					{(taskManager.tasksData.memory.freeHeap / 1024).toFixed(1)} KB
				</div>
			</div>

			<div class="rounded-box bg-base-200 py-2 px-1 flex flex-col items-center justify-center">
				<div class="text-[10px] font-bold opacity-60 uppercase tracking-wider">
					{m.task_manager_heap({ locale: i18n.languageTag })}
				</div>
				<div class="text-lg font-bold text-warning leading-tight">
					{(taskManager.tasksData.memory.minFreeHeap / 1024).toFixed(1)} KB
				</div>
			</div>

			<div class="rounded-box bg-base-200 py-2 px-1 flex flex-col items-center justify-center">
				<div class="text-[10px] font-bold opacity-60 uppercase tracking-wider">
					{m.task_manager_psram({ locale: i18n.languageTag })}
				</div>
				<div class="text-lg font-bold text-info leading-tight">
					{taskManager.tasksData.memory.freePsram
						? (taskManager.tasksData.memory.freePsram / 1024).toFixed(1) + ' KB'
						: m.task_manager_na({ locale: i18n.languageTag })}
				</div>
			</div>
		</div>

		{#if taskManager.tasksData.error}
			<div class="mb-3 alert alert-error">
				<span>
					{m.error_prefix(
						{ error: taskManager.tasksData.error ?? '' },
						{ locale: i18n.languageTag }
					)}
				</span>
			</div>
		{/if}

		{#if taskManager.hasDetails && taskManager.tasksData.tasks}
			<div class="rounded-box max-h-[60vh] overflow-x-auto bg-base-200 scrollbar-thin">
				<DataTable size="xs" pinRows>
					<thead>
						<tr>
							<th>{m.task_manager_col_name({ locale: i18n.languageTag })}</th>
							<th>{m.task_manager_col_state({ locale: i18n.languageTag })}</th>
							<th class="text-center">{m.task_manager_col_prio({ locale: i18n.languageTag })}</th>
							<th class="text-center">{m.task_manager_col_core({ locale: i18n.languageTag })}</th>
							<th class="text-right pr-4">
								{m.task_manager_col_stack({ locale: i18n.languageTag })}
							</th>
						</tr>
					</thead>
					<tbody>
						{#each taskManager.tasksData.tasks as task, i (i)}
							<tr class="hover:bg-base-100">
								<td class="font-mono font-bold text-xs">{task.name}</td>
								<td>
									<span
										class="badge badge-xs {getStateBadge(
											task.state
										)} uppercase font-bold text-[10px]"
									>
										{getStateLabel(task.state)}
									</span>
								</td>
								<td class="text-center">
									<span class="{getPriorityClass(task.priority)} font-mono">{task.priority}</span>
								</td>
								<td class="text-center">
									<span
										class="badge badge-xs font-bold text-[10px] {task.coreId === 1
											? 'badge-secondary'
											: task.coreId === 0
												? 'badge-primary'
												: 'badge-ghost'}"
									>
										{task.coreId === -1
											? m.task_manager_core_any({ locale: i18n.languageTag })
											: (task.coreId ?? '-')}
									</span>
								</td>
								<td class="text-right pr-4">
									<span
										class="font-mono text-xs tabular-nums {getStackColorClass(
											task.stackHighWaterMark
										)}"
									>
										{(task.stackHighWaterMark ?? 0).toLocaleString()} B
									</span>
								</td>
							</tr>
						{/each}
					</tbody>
				</DataTable>
			</div>
		{/if}
	{/if}

	{#snippet actions()}
		<div class="flex justify-end w-full">
			<FormButton
				label={m.action_close({ locale: i18n.languageTag })}
				icon={X}
				onclick={() => (open = false)}
				class="btn-neutral"
			/>
		</div>
	{/snippet}
</Modal>
