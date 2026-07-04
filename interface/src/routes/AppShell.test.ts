import { cleanup, render } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import AppShell from './AppShell.svelte';
import type { SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

const { mockSystemStatus, mockApiClient, getUnauthorizedHandler } = vi.hoisted(() => {
	let unauthorizedHandler: (() => void) | null = null;

	return {
		mockSystemStatus: {
			subscribeChannel: vi.fn(),
			unsubscribeChannel: vi.fn()
		},
		mockApiClient: {
			setGlobalUnauthorizedHandler: vi.fn((handler: () => void) => {
				unauthorizedHandler = handler;
			})
		},
		getUnauthorizedHandler: () => unauthorizedHandler
	};
});

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: mockSystemStatus
}));

vi.mock('$lib/utils/api/apiClient', () => ({
	setGlobalUnauthorizedHandler: mockApiClient.setGlobalUnauthorizedHandler
}));

vi.mock('./menu.svelte', async () => {
	const { default: EmptyComponent } = await import('$lib/testing/EmptyComponent.svelte');
	return { default: EmptyComponent };
});

vi.mock('./statusbar.svelte', async () => {
	const { default: EmptyComponent } = await import('$lib/testing/EmptyComponent.svelte');
	return { default: EmptyComponent };
});

describe('AppShell', () => {
	afterEach(() => {
		cleanup();
	});

	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('acquires and releases the core system status lease around shell lifetime', () => {
		const session = {
			invalidate: vi.fn()
		} as unknown as SessionAccess;

		const view = render(AppShell, { props: { session } });

		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('system_status');

		view.unmount();

		expect(mockSystemStatus.unsubscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockSystemStatus.unsubscribeChannel).toHaveBeenCalledWith('system_status');
	});

	it('routes the global unauthorized handler through session invalidation', () => {
		const session = {
			invalidate: vi.fn()
		} as unknown as SessionAccess;

		render(AppShell, { props: { session } });
		getUnauthorizedHandler()?.();

		expect(mockApiClient.setGlobalUnauthorizedHandler).toHaveBeenCalledTimes(1);
		expect(session.invalidate).toHaveBeenCalledWith('unauthorized');
	});
});
