import { onDestroy } from 'svelte';
import { SystemApiService, type TasksResponse } from '$lib/services/api/core/SystemApiService';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

interface TaskManagerDeps {
	createApi?: () => Pick<SystemApiService, 'getTasks'>;
	isOpen?: () => boolean;
}

export function useTaskManager(deps: TaskManagerDeps = {}) {
	const { createService } = useApiClient();

	let tasksData = $state<TasksResponse | null>(null);
	let loading = $state(false);
	let error = $state<string | null>(null);

	let activeController: AbortController | null = null;
	let requestId = 0;

	const isOpen = deps.isOpen ?? (() => false);

	function getApi() {
		return deps.createApi?.() ?? createService(SystemApiService);
	}

	function cancelPending() {
		activeController?.abort();
		activeController = null;
	}

	async function fetchTasks() {
		cancelPending();

		const controller = new AbortController();
		const currentRequestId = ++requestId;
		activeController = controller;
		loading = true;
		error = null;

		try {
			const nextTasks = await getApi().getTasks({
				signal: controller.signal,
				details: true
			});
			if (controller.signal.aborted || currentRequestId !== requestId) return;
			tasksData = nextTasks;
		} catch (cause) {
			if (getRequestAbortKind(cause) === 'abort') return;
			if (currentRequestId !== requestId) return;
			error = toUserRequestErrorMessage(cause);
		} finally {
			if (currentRequestId === requestId) {
				loading = false;
			}
			if (activeController === controller) {
				activeController = null;
			}
		}
	}

	$effect(() => {
		if (!isOpen()) {
			cancelPending();
			return;
		}

		void fetchTasks();
		return () => {
			cancelPending();
		};
	});

	onDestroy(() => {
		cancelPending();
	});

	return {
		get tasksData() {
			return tasksData;
		},
		get hasDetails() {
			return Boolean(tasksData?.tasks?.length);
		},
		get loading() {
			return loading;
		},
		get error() {
			return error;
		},
		fetchTasks,
		refresh: fetchTasks
	};
}
