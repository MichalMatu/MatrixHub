<script module lang="ts">
	import { writable } from 'svelte/store';

	type NotificationType = 'info' | 'success' | 'warning' | 'error';

	type Notification = {
		id: string;
		type: NotificationType;
		message: string;
	};

	function createNotificationStore() {
		const { subscribe, update } = writable<Notification[]>([]);

		function remove(id: string) {
			update((items) => items.filter((item) => item.id !== id));
		}

		function send(message: string, type: NotificationType = 'info', timeout: number) {
			const id = generateId();
			update((items) => [...items, { id, type, message }]);
			setTimeout(() => remove(id), timeout);
		}

		return {
			subscribe,
			send,
			error: (message: string, timeout: number) => send(message, 'error', timeout),
			warning: (message: string, timeout: number) => send(message, 'warning', timeout),
			info: (message: string, timeout: number) => send(message, 'info', timeout),
			success: (message: string, timeout: number) => send(message, 'success', timeout)
		};
	}

	function generateId() {
		return `_${Math.random().toString(36).slice(2, 11)}`;
	}

	export const notifications = createNotificationStore();
</script>
