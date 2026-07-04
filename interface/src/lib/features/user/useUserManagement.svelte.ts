import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { ConfirmDialog } from '$lib/components';
import {
	SecurityApiService,
	type SecuritySettings,
	type UserSetting
} from '$lib/services/api/core/SecurityApiService';
import { Logger } from '$lib/services/core/Logger';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { confirm } from '$lib/utils/ui/dialogs';
import { openModal, type ModalOpenService } from '$lib/utils/ui/modal';
import Cancel from '~icons/tabler/x';
import Check from '~icons/tabler/check';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type { Component } from 'svelte';

export interface UserManagementState {
	securitySettings: SecuritySettings;
	isLoading: boolean;
	isSaving: boolean;
	error: string | null;
}

type UserSecurityApi = Pick<SecurityApiService, 'getSecuritySettings' | 'saveSecuritySettings'>;
type UserEditModalProps = {
	isOpen: boolean;
	title: string;
	onSaveUser: (user: UserSetting) => void;
	user?: UserSetting;
};
type UserEditModalComponent = Component<UserEditModalProps>;

interface UserNotifications {
	success(message: string, duration?: number): void;
	error(message: string, duration?: number): void;
}

type UserModalService = ModalOpenService;

interface UserManagementDeps {
	createSecurityApi?: () => UserSecurityApi;
	generateSecret?: () => string;
	invalidateCurrentUser?: () => void;
	modalService?: UserModalService;
	notifications?: UserNotifications;
	shouldLoad?: () => boolean;
}

function createEmptySecuritySettings(): SecuritySettings {
	return {
		jwt_secret: '',
		jwt_secret_configured: false,
		users: []
	};
}

function cloneSecuritySettings(settings: SecuritySettings): SecuritySettings {
	return {
		jwt_secret: typeof settings.jwt_secret === 'string' ? settings.jwt_secret : '',
		jwt_secret_configured: !!settings.jwt_secret_configured,
		users: settings.users.map((entry) => ({ ...entry }))
	};
}

function parseStoredSecuritySettings(serialized: string): SecuritySettings {
	try {
		const parsed = JSON.parse(serialized) as Partial<SecuritySettings> | null;
		if (!parsed || typeof parsed !== 'object') {
			return createEmptySecuritySettings();
		}

		return {
			jwt_secret: typeof parsed.jwt_secret === 'string' ? parsed.jwt_secret : '',
			jwt_secret_configured: !!parsed.jwt_secret_configured,
			users: Array.isArray(parsed.users)
				? parsed.users.map((entry) => ({
						username: typeof entry?.username === 'string' ? entry.username : '',
						password: typeof entry?.password === 'string' ? entry.password : '',
						admin: !!entry?.admin
					}))
				: []
		};
	} catch {
		return createEmptySecuritySettings();
	}
}

function withUsers(settings: SecuritySettings, users: UserSetting[]): SecuritySettings {
	return {
		...cloneSecuritySettings(settings),
		users: users.map((entry) => ({ ...entry }))
	};
}

function findUser(settings: SecuritySettings, username: string): UserSetting | undefined {
	return settings.users.find((entry) => entry.username === username);
}

function shouldInvalidateCurrentSession(
	previousSettings: SecuritySettings,
	nextSettings: SecuritySettings,
	currentUsername: string,
	hasBearerToken: boolean
): boolean {
	if (!hasBearerToken || !currentUsername) return false;
	if (previousSettings.jwt_secret !== nextSettings.jwt_secret) return true;

	const nextUser = findUser(nextSettings, currentUsername);
	if (!nextUser) return true;

	const previousUser = findUser(previousSettings, currentUsername);
	if (!previousUser) return false;

	return previousUser.password !== nextUser.password || previousUser.admin !== nextUser.admin;
}

export function useUserManagement(
	EditUserModal: UserEditModalComponent,
	deps: UserManagementDeps = {}
) {
	const apiClient = deps.createSecurityApi ? null : useApiClient();
	const toast = deps.notifications ?? notifications;
	const modalService = deps.modalService;
	const session = useSessionAccess();

	let state = $state<UserManagementState>({
		securitySettings: createEmptySecuritySettings(),
		isLoading: false,
		isSaving: false,
		error: null
	});

	let originalSettingsStr = $state(JSON.stringify(createEmptySecuritySettings()));
	const isDirty = $derived(JSON.stringify(state.securitySettings) !== originalSettingsStr);

	function getSecurityApiService(): UserSecurityApi {
		return deps.createSecurityApi?.() ?? apiClient!.createService(SecurityApiService);
	}

	function setSavedSettings(settings: SecuritySettings): void {
		const clonedSettings = cloneSecuritySettings(settings);
		state.securitySettings = clonedSettings;
		originalSettingsStr = JSON.stringify(clonedSettings);
		state.error = null;
	}

	let autoLoadArmed = true;

	$effect(() => {
		const shouldLoad = deps.shouldLoad?.();
		if (shouldLoad === undefined) return;
		if (!shouldLoad) {
			autoLoadArmed = true;
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;

		void loadSettings();
	});

	async function loadSettings(): Promise<void> {
		if (state.isLoading) return;
		state.isLoading = true;
		state.error = null;
		try {
			const settings = await getSecurityApiService().getSecuritySettings();
			setSavedSettings(settings);
		} catch (error) {
			Logger.error('User management error:', error);
			const kind = getRequestAbortKind(error);
			if (kind === 'abort') return;
			state.error = toUserRequestErrorMessage(error, {
				timeoutMessage: m.toast_user_settings_load_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.toast_user_settings_load_failed({ locale: i18n.languageTag })
			});
		} finally {
			state.isLoading = false;
		}
	}

	async function saveSettings(data: SecuritySettings): Promise<void> {
		if (state.isSaving) return;
		state.isSaving = true;
		try {
			const previousSettings = parseStoredSecuritySettings(originalSettingsStr);
			const requestedSettings = cloneSecuritySettings(data);
			const updatedSettings = await getSecurityApiService().saveSecuritySettings(requestedSettings);
			const invalidateCurrentSession =
				shouldInvalidateCurrentSession(
					previousSettings,
					requestedSettings,
					session.username,
					!!session.bearerToken
				) ||
				shouldInvalidateCurrentSession(
					previousSettings,
					updatedSettings,
					session.username,
					!!session.bearerToken
				);
			setSavedSettings(updatedSettings);

			if (invalidateCurrentSession) {
				if (deps.invalidateCurrentUser) {
					deps.invalidateCurrentUser();
				} else {
					session.invalidate();
				}
				return;
			}

			toast.success(m.user_msg_updated({ locale: i18n.languageTag }), 3000);
		} catch (error) {
			Logger.error('User management error:', error);
			const kind = getRequestAbortKind(error);
			if (kind === 'abort') return;
			const msg = toUserRequestErrorMessage(error, {
				timeoutMessage: m.toast_user_settings_save_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.toast_user_settings_save_failed({ locale: i18n.languageTag })
			});
			state.error = msg;
			toast.error(m.toast_message({ message: msg }, { locale: i18n.languageTag }), 5000);
		} finally {
			state.isSaving = false;
		}
	}

	function resetSettings(): void {
		state.securitySettings = parseStoredSecuritySettings(originalSettingsStr);
		state.error = null;
	}

	function openDeleteConfirmation(index: number): void {
		const currentUser = state.securitySettings.users[index];
		if (!currentUser) return;

		confirm({
			title: m.user_dialog_delete_title({ locale: i18n.languageTag }),
			message: m.user_dialog_delete_msg(
				{ name: currentUser.username },
				{ locale: i18n.languageTag }
			),
			labels: {
				cancel: { label: m.action_abort({ locale: i18n.languageTag }), icon: Cancel },
				confirm: { label: m.action_yes({ locale: i18n.languageTag }), icon: Check }
			},
			onConfirm: () => {
				const nextSettings = withUsers(
					state.securitySettings,
					state.securitySettings.users.filter((_, userIndex) => userIndex !== index)
				);
				void saveSettings(nextSettings);
			},
			component: ConfirmDialog,
			modalService
		});
	}

	function openEditModal(index: number): void {
		const currentUser = state.securitySettings.users[index];
		if (!currentUser) return;

		openModal(
			EditUserModal,
			{
				title: m.user_dialog_edit_title({ locale: i18n.languageTag }),
				user: { ...currentUser },
				onSaveUser: (editedUser: UserSetting) => {
					const nextSettings = withUsers(
						state.securitySettings,
						state.securitySettings.users.map((existingUser, userIndex) =>
							userIndex === index ? editedUser : existingUser
						)
					);
					void saveSettings(nextSettings);
				}
			},
			modalService
		);
	}

	function openNewUserModal(): void {
		openModal(
			EditUserModal,
			{
				title: m.user_dialog_add_title({ locale: i18n.languageTag }),
				onSaveUser: (newUser: UserSetting) => {
					const nextSettings = withUsers(state.securitySettings, [
						...state.securitySettings.users,
						newUser
					]);
					void saveSettings(nextSettings);
				}
			},
			modalService
		);
	}

	function generateJwtSecret(): void {
		const nextSecret = deps.generateSecret?.() ?? crypto.randomUUID();
		state.securitySettings = {
			...state.securitySettings,
			jwt_secret: nextSecret,
			jwt_secret_configured: true
		};
	}

	return {
		get state() {
			return state;
		},
		get isDirty() {
			return isDirty;
		},
		loadSettings,
		saveSettings,
		resetSettings,
		openDeleteConfirmation,
		openEditModal,
		openNewUserModal,
		generateJwtSecret
	};
}
