export function createPersistentLanguage(key: string, initialValue: string) {
	// Initialize with default or passed value (safe for SSR)
	let currentValue = initialValue;

	// Synchronously checking storage in browser environment
	if (typeof window !== 'undefined') {
		const stored = localStorage.getItem(key);
		if (stored) {
			currentValue = stored;
		}
	}

	// Create state
	let state = $state(currentValue);

	return {
		get value() {
			return state;
		},
		set value(v: string) {
			state = v;
			if (typeof window !== 'undefined') {
				localStorage.setItem(key, v);
			}
		}
	};
}
