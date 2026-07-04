import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { AlarmRule } from '$lib/types/domain/alarms';

const { loggerError, mockNotifications, mockModalService, mockSystemStatus, eventBus } = vi.hoisted(
	() => {
		let _subscriber: ((value: unknown) => void) | null = null;

		return {
			loggerError: vi.fn(),
			mockNotifications: {
				success: vi.fn(),
				error: vi.fn()
			},
			mockModalService: {
				open: vi.fn()
			},
			mockSystemStatus: {
				subscribeChannel: vi.fn(),
				unsubscribeChannel: vi.fn(),
				getSnapshot: vi.fn<(channel?: string) => unknown | null>(() => null)
			},
			eventBus: {
				subscribe(run: (value: unknown) => void) {
					_subscriber = run;
					run(null);
					return () => {
						_subscriber = null;
					};
				}
			}
		};
	}
);

vi.mock('svelte', async (importOriginal) => {
	const actual = await importOriginal();
	return {
		// @ts-expect-error test override
		...actual,
		onMount: (fn: () => void) => fn(),
		onDestroy: vi.fn()
	};
});

vi.mock('svelte-modals', () => ({
	modals: {
		open: vi.fn()
	}
}));

vi.mock('$lib/components', () => ({
	ConfirmDialog: {}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: vi.fn(
		(
			options: { modalService?: typeof mockModalService; component?: unknown } & Record<
				string,
				unknown
			>
		) => {
			const { modalService = mockModalService, component = {}, ...props } = options;
			return modalService.open(component, props);
		}
	)
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn((error: unknown) =>
		typeof error === 'object' && error !== null && 'kind' in error
			? (error as { kind: string }).kind
			: null
	),
	toUserRequestErrorMessage: vi.fn(
		(error: unknown, options?: { fallbackMessage?: string; timeoutMessage?: string }) => {
			if (error instanceof Error && error.message) return error.message;
			return options?.fallbackMessage ?? 'unknown';
		}
	)
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: loggerError
	}
}));

vi.mock('$lib/stores/user', () => ({
	user: {
		username: 'tester',
		admin: true,
		bearer_token: 'token',
		isValid: true,
		invalidate: vi.fn()
	}
}));

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: vi.fn()
	})
}));

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: mockSystemStatus,
	systemEvents: eventBus
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	alarms_error_load_timeout: () => 'load timeout',
	alarms_error_load_fallback: () => 'load failed',
	alarms_error_save_timeout: () => 'save timeout',
	alarms_error_save_fallback: () => 'save failed',
	alarms_error_duplicate_name: () => 'duplicate name',
	alarms_error_update: () => 'update failed',
	alarms_max_rules_reached: ({ max }: { max: number }) => `max ${max} rules reached`,
	alarms_save_success: () => 'saved',
	alarms_delete_success: () => 'deleted',
	alarms_delete_title: () => 'Delete alarm',
	alarms_delete_msg: ({ name }: { name: string }) => `delete ${name}`,
	error_prefix: ({ error }: { error: string }) => `error: ${error}`
}));

function createRule(overrides: Partial<AlarmRule> = {}): AlarmRule {
	return {
		id: 'alarm-1',
		enabled: true,
		name: 'High temp',
		source: 'temperature',
		operator: 'above',
		threshold: 30,
		severity: 'warning',
		notify_channels: ['telegram'],
		cooldown_seconds: 600,
		shelly_device_ids: [],
		created_at: 100,
		updated_at: 100,
		...overrides
	};
}

function flushPromises() {
	return new Promise((resolve) => setTimeout(resolve, 0));
}

function createAccess(canRead: boolean, canManage: boolean) {
	return { canRead, canManage };
}

describe('useAlarmsManagement', () => {
	beforeEach(async () => {
		vi.clearAllMocks();
		const { alarmsStore } = await import('$lib/stores/alarms.svelte');
		alarmsStore.reset();
	});

	it('skips API calls when user cannot manage alarms', async () => {
		const { useAlarmsManagement } = await import('./useAlarmsManagement.svelte');
		const api = {
			getRules: vi.fn(),
			saveRules: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const alarms = useAlarmsManagement({
					access: createAccess(false, false),
					createApi: () => api as never
				});

				void vi
					.waitFor(() => {
						expect(api.getRules).not.toHaveBeenCalled();
						expect(alarms.loading).toBe(false);
						expect(alarms.rules).toEqual([]);
					})
					.then(() => {
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('loads rules for admin users', async () => {
		const { useAlarmsManagement } = await import('./useAlarmsManagement.svelte');
		const api = {
			getRules: vi.fn().mockResolvedValue({ schema_version: 1, rules: [createRule()] }),
			saveRules: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const alarms = useAlarmsManagement({
					access: createAccess(true, true),
					createApi: () => api as never
				});

				void vi
					.waitFor(() => {
						expect(api.getRules).toHaveBeenCalledOnce();
						expect(alarms.rules).toHaveLength(1);
						expect(alarms.error).toBeNull();
					})
					.then(() => {
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('updates rule toggle with injected timestamp and reloads rules after save failure', async () => {
		const { useAlarmsManagement } = await import('./useAlarmsManagement.svelte');
		const serverRule = createRule({ enabled: true, updated_at: 555 });
		const api = {
			getRules: vi
				.fn()
				.mockResolvedValueOnce({ schema_version: 1, rules: [createRule()] })
				.mockResolvedValueOnce({ schema_version: 1, rules: [serverRule] }),
			saveRules: vi.fn().mockRejectedValue(new Error('save failed'))
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const alarms = useAlarmsManagement({
					access: createAccess(true, true),
					createApi: () => api as never,
					now: () => 999
				});

				void vi
					.waitFor(() => {
						expect(alarms.rules).toHaveLength(1);
					})
					.then(async () => {
						alarms.toggleRule(createRule());
						await flushPromises();

						expect(api.saveRules).toHaveBeenCalledWith({
							schema_version: 1,
							rules: [expect.objectContaining({ enabled: false, updated_at: 999 })]
						});
						expect(api.getRules).toHaveBeenCalledTimes(2);
						expect(alarms.rules).toEqual([serverRule]);
						expect(alarms.error).toBeNull();
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('blocks opening add modal when the maximum number of rules is reached', async () => {
		const { useAlarmsManagement } = await import('./useAlarmsManagement.svelte');
		const api = {
			getRules: vi.fn().mockResolvedValue({
				schema_version: 1,
				rules: Array.from({ length: 8 }, (_, index) =>
					createRule({ id: `alarm-${index}`, name: `Alarm ${index}` })
				)
			}),
			saveRules: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const alarms = useAlarmsManagement({
					access: createAccess(true, true),
					createApi: () => api as never
				});

				void vi
					.waitFor(() => {
						expect(alarms.rules).toHaveLength(8);
					})
					.then(() => {
						alarms.openAddModal();

						expect(alarms.showModal).toBe(false);
						expect(alarms.maxRulesReached).toBe(true);
						expect(alarms.error).toBe('max 8 rules reached');
						expect(mockNotifications.error).toHaveBeenCalledWith('max 8 rules reached', 4000);
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('rejects duplicate rule names before saving', async () => {
		const { useAlarmsManagement } = await import('./useAlarmsManagement.svelte');
		const existingRule = createRule({ id: 'alarm-1', name: 'High temp' });
		const api = {
			getRules: vi.fn().mockResolvedValue({ schema_version: 1, rules: [existingRule] }),
			saveRules: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const alarms = useAlarmsManagement({
					access: createAccess(true, true),
					createApi: () => api as never
				});

				void vi
					.waitFor(() => {
						expect(alarms.rules).toHaveLength(1);
					})
					.then(async () => {
						await alarms.submitRule(createRule({ id: 'alarm-2', name: ' high temp ' }));

						expect(api.saveRules).not.toHaveBeenCalled();
						expect(alarms.error).toBe('duplicate name');
						expect(mockNotifications.error).toHaveBeenCalledWith('duplicate name', 4000);
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('opens delete confirmation and removes rule after confirm', async () => {
		const { useAlarmsManagement } = await import('./useAlarmsManagement.svelte');
		const remainingRule = createRule({ id: 'alarm-2', name: 'Low humidity', source: 'humidity' });
		const api = {
			getRules: vi
				.fn()
				.mockResolvedValue({ schema_version: 1, rules: [createRule(), remainingRule] }),
			saveRules: vi.fn().mockResolvedValue({ schema_version: 1, rules: [remainingRule] })
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const alarms = useAlarmsManagement({
					access: createAccess(true, true),
					createApi: () => api as never,
					modalService: mockModalService
				});

				void vi
					.waitFor(() => {
						expect(alarms.rules).toHaveLength(2);
					})
					.then(async () => {
						alarms.confirmDelete(createRule());

						const payload = mockModalService.open.mock.calls[0]?.[1] as { onConfirm?: () => void };
						payload.onConfirm?.();
						await flushPromises();

						expect(api.saveRules).toHaveBeenCalledWith({
							schema_version: 1,
							rules: [remainingRule]
						});
						expect(alarms.rules).toEqual([remainingRule]);
						expect(mockNotifications.success).toHaveBeenCalledWith('deleted', 3000);
						resolve();
					});
			});
		});

		cleanup?.();
	});

	it('loads rules in read-only mode but blocks write actions', async () => {
		const { useAlarmsManagement } = await import('./useAlarmsManagement.svelte');
		const initialRule = createRule();
		const api = {
			getRules: vi.fn().mockResolvedValue({ schema_version: 1, rules: [initialRule] }),
			saveRules: vi.fn()
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const alarms = useAlarmsManagement({
					access: createAccess(true, false),
					createApi: () => api as never,
					modalService: mockModalService,
					now: () => 999
				});

				void vi
					.waitFor(() => {
						expect(api.getRules).toHaveBeenCalledOnce();
					})
					.then(async () => {
						alarms.toggleRule(initialRule);
						alarms.openAddModal();
						alarms.openEditModal(initialRule);
						alarms.confirmDelete(initialRule);
						await flushPromises();

						expect(api.getRules).toHaveBeenCalledOnce();
						expect(alarms.rules).toEqual([initialRule]);
						expect(alarms.showModal).toBe(false);
						expect(api.saveRules).not.toHaveBeenCalled();
						expect(mockModalService.open).not.toHaveBeenCalled();
						resolve();
					});
			});
		});

		cleanup?.();
	});
});
