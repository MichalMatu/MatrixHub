import { beforeEach, describe, expect, it, vi } from 'vitest';

const {
	mockSubscribeChannel,
	mockUnsubscribeChannel,
	mockGetSnapshot,
	mockRequestSnapshot,
	mockEventSubscribe,
	mockEventUnsubscribe,
	emitEvent
} = vi.hoisted(() => {
	let eventCallback: ((value: unknown) => void) | null = null;

	return {
		mockSubscribeChannel: vi.fn(),
		mockUnsubscribeChannel: vi.fn(),
		mockGetSnapshot: vi.fn(),
		mockRequestSnapshot: vi.fn(),
		mockEventUnsubscribe: vi.fn(),
		mockEventSubscribe: vi.fn((callback: (value: unknown) => void) => {
			eventCallback = callback;
			return mockEventUnsubscribe;
		}),
		emitEvent: (value: unknown) => eventCallback?.(value)
	};
});

vi.mock('../systemStatus.svelte', () => ({
	systemStatus: {
		subscribeChannel: mockSubscribeChannel,
		unsubscribeChannel: mockUnsubscribeChannel,
		getSnapshot: mockGetSnapshot,
		requestSnapshot: mockRequestSnapshot,
		subscribeEvents: mockEventSubscribe
	}
}));

describe('createSystemChannelSubscription', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockGetSnapshot.mockReturnValue(null);
	});

	it('hydrates the cached snapshot before sending subscribe', async () => {
		mockGetSnapshot.mockReturnValue({ value: 42 });
		const onSnapshot = vi.fn();
		const { createSystemChannelSubscription } = await import('./channelSubscription.svelte');
		const channel = createSystemChannelSubscription<{ value: number }>({
			channel: 'telemetry',
			onSnapshot
		});

		const hasCachedSnapshot = channel.subscribe();

		expect(hasCachedSnapshot).toBe(true);
		expect(onSnapshot).toHaveBeenCalledWith({ value: 42 });
		expect(mockSubscribeChannel).toHaveBeenCalledWith('telemetry');
		expect(mockEventSubscribe).toHaveBeenCalledTimes(1);
	});

	it('accepts a snapshot emitted immediately while subscribeChannel is running', async () => {
		const onSnapshot = vi.fn();
		mockSubscribeChannel.mockImplementationOnce(() => {
			emitEvent({
				type: 'snapshot',
				channel: 'telemetry',
				data: { value: 7 }
			});
		});
		const { createSystemChannelSubscription } = await import('./channelSubscription.svelte');
		const channel = createSystemChannelSubscription<{ value: number }>({
			channel: 'telemetry',
			onSnapshot
		});

		channel.subscribe({ hydrateSnapshot: false });

		expect(onSnapshot).toHaveBeenCalledTimes(1);
		expect(onSnapshot).toHaveBeenCalledWith({ value: 7 });
	});

	it('routes only matching snapshots to onSnapshot and live events to onEvent', async () => {
		const onSnapshot = vi.fn();
		const onEvent = vi.fn();
		const { createSystemChannelSubscription } = await import('./channelSubscription.svelte');
		const channel = createSystemChannelSubscription<{ value: number }>({
			channel: 'telemetry',
			onSnapshot,
			onEvent
		});

		channel.subscribe({ hydrateSnapshot: false });

		emitEvent({
			type: 'snapshot',
			channel: 'other',
			data: { value: 1 }
		});
		emitEvent({
			type: 'snapshot',
			channel: 'telemetry',
			data: { value: 2 }
		});
		emitEvent({
			type: 'sensor',
			data: { temp: 21 }
		});

		expect(onSnapshot).toHaveBeenCalledTimes(1);
		expect(onSnapshot).toHaveBeenCalledWith({ value: 2 });
		expect(onEvent).toHaveBeenCalledTimes(1);
		expect(onEvent).toHaveBeenCalledWith({
			type: 'sensor',
			data: { temp: 21 }
		});
	});

	it('keeps the event subscription stable across repeated subscribe calls and cleans up on destroy', async () => {
		const { createSystemChannelSubscription } = await import('./channelSubscription.svelte');
		const channel = createSystemChannelSubscription({
			channel: 'system_status'
		});

		channel.subscribe();
		channel.subscribe();
		channel.destroy();

		expect(mockEventSubscribe).toHaveBeenCalledTimes(1);
		expect(mockSubscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockSubscribeChannel).toHaveBeenCalledWith('system_status');
		expect(mockUnsubscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockUnsubscribeChannel).toHaveBeenCalledWith('system_status');
		expect(mockEventUnsubscribe).toHaveBeenCalledTimes(1);
	});

	it('refreshes a subscribed channel without acquiring another managed lease', async () => {
		mockGetSnapshot.mockReturnValue({ value: 9 });
		const onSnapshot = vi.fn();
		const { createSystemChannelSubscription } = await import('./channelSubscription.svelte');
		const channel = createSystemChannelSubscription({
			channel: 'system_status',
			onSnapshot
		});

		channel.subscribe({ hydrateSnapshot: false });
		const hasCachedSnapshot = channel.refresh({ hydrateSnapshot: true });
		channel.destroy();

		expect(hasCachedSnapshot).toBe(true);
		expect(onSnapshot).toHaveBeenCalledWith({ value: 9 });
		expect(mockSubscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockRequestSnapshot).toHaveBeenCalledTimes(1);
		expect(mockRequestSnapshot).toHaveBeenCalledWith('system_status');
		expect(mockUnsubscribeChannel).toHaveBeenCalledTimes(1);
	});

	it('ignores new events after unsubscribe until the channel is subscribed again', async () => {
		const onEvent = vi.fn();
		const { createSystemChannelSubscription } = await import('./channelSubscription.svelte');
		const channel = createSystemChannelSubscription({
			channel: 'sensing',
			onEvent
		});

		channel.subscribe({ hydrateSnapshot: false });
		channel.unsubscribe();

		emitEvent({
			type: 'sensing',
			data: { variance: 1.2 }
		});

		expect(onEvent).not.toHaveBeenCalled();

		channel.subscribe({ hydrateSnapshot: false });
		emitEvent({
			type: 'sensing',
			data: { variance: 2.4 }
		});

		expect(onEvent).toHaveBeenCalledTimes(1);
		expect(onEvent).toHaveBeenCalledWith({
			type: 'sensing',
			data: { variance: 2.4 }
		});
	});
});
