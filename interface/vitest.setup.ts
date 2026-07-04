import { afterEach, vi } from 'vitest';
import { cleanup } from '@testing-library/svelte';

// Set Svelte 5 runes mode
declare global {
	var $$IS_LEGACY: boolean;
}
globalThis.$$IS_LEGACY = false;

// Mock matchMedia for uPlot / local storage wrapper
Object.defineProperty(window, 'matchMedia', {
	writable: true,
	value: vi.fn().mockImplementation((query) => ({
		matches: false,
		media: query,
		onchange: null,
		addListener: vi.fn(), // deprecated
		removeListener: vi.fn(), // deprecated
		addEventListener: vi.fn(),
		removeEventListener: vi.fn(),
		dispatchEvent: vi.fn()
	}))
});

Object.defineProperty(Element.prototype, 'animate', {
	writable: true,
	value: vi.fn().mockImplementation(() => ({
		cancel: vi.fn(),
		finish: vi.fn(),
		play: vi.fn(),
		pause: vi.fn(),
		addEventListener: vi.fn(),
		removeEventListener: vi.fn(),
		onfinish: null
	}))
});

// Cleanup after each test
afterEach(() => {
	cleanup();
});

// Mock SvelteKit modules
vi.mock('$app/navigation', () => ({
	goto: vi.fn(),
	invalidate: vi.fn(),
	invalidateAll: vi.fn(),
	preloadData: vi.fn(),
	preloadCode: vi.fn(),
	beforeNavigate: vi.fn(),
	afterNavigate: vi.fn()
}));
