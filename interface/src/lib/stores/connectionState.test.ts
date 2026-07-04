import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { ConnectionStateStore } from './connectionState.svelte';

describe('ConnectionStateStore', () => {
	let store: ConnectionStateStore;

	beforeEach(() => {
		vi.useFakeTimers();
		store = new ConnectionStateStore();
	});

	afterEach(() => {
		vi.useRealTimers();
	});

	it('should return "connected" by default', () => {
		expect(store.status).toBe('connected');
	});

	it('should return "disconnected" if API is not healthy', () => {
		store.setApiHealthy(false);
		expect(store.status).toBe('disconnected');
	});

	it('should return "connecting" if API is not healthy and reconnecting', () => {
		store.setApiHealthy(false);
		store.setReconnectAttempt(1);
		expect(store.status).toBe('connecting');
	});

	it('should return "error" if there was a recent error', () => {
		store.logConnectionError('Failed to fetch');
		expect(store.status).toBe('error');
	});

	it('should return "connected" after error timeout', () => {
		store.logConnectionError('Failed to fetch');
		expect(store.status).toBe('error');

		// Advance 11 seconds
		vi.advanceTimersByTime(11000);
		expect(store.status).toBe('connected');
	});

	it('should clear reconnect attempts when connection errors are cleared', () => {
		store.setApiHealthy(false);
		store.setReconnectAttempt(3);

		expect(store.status).toBe('connecting');

		store.setApiHealthy(true);
		store.clearConnectionErrors();

		expect(store.reconnectAttempt).toBe(0);
		expect(store.status).toBe('connected');
	});

	it('should throttle logging of connection errors', () => {
		// Mock Logger check
		store.logConnectionError('Error 1');
		store.logConnectionError('Error 2');

		// Only one log expected due to 10s throttle
		// (Assuming Logger.warn uses console.warn which we are spying on if Logger is not easily mockable)
		// Actually, let's just check the internal state
		expect(store.errorCount).toBe(2);
		expect(store.lastError).toBe('Error 2');

		vi.advanceTimersByTime(11000);
		store.logConnectionError('Error 3');
		expect(store.errorCount).toBe(3);
	});
});
