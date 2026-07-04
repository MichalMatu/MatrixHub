import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

export function formatMs(ms: number): string {
	if (ms < 1000) return m.duration_sleeping_soon({ locale: i18n.languageTag });
	if (ms < 60000) return m.duration_seconds({ seconds: (ms / 1000).toFixed(1) });
	if (ms < 3600000) return m.duration_minutes({ minutes: (ms / 60000).toFixed(1) });
	return m.duration_hours({ hours: (ms / 3600000).toFixed(1) });
}
