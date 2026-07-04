import { notifications } from '$lib/components/toasts/notifications.svelte';

type NotificationType = 'success' | 'error' | 'warning' | 'info';

let lastMessage = '';
let lastTime = 0;
const DEDUP_WINDOW_MS = 1000;

const showNotification = (type: NotificationType, message: string, duration = 3000): void => {
	const now = Date.now();
	if (message === lastMessage && now - lastTime < DEDUP_WINDOW_MS) return;
	lastMessage = message;
	lastTime = now;
	return notifications[type](message, duration);
};

export const showSuccess = (message: string, duration = 3000): void => {
	showNotification('success', message, duration);
};

export const showError = (message: string, duration = 3000): void => {
	showNotification('error', message, duration);
};
