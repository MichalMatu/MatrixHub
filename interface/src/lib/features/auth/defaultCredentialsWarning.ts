import { goto } from '$app/navigation';
import { confirm } from '$lib/utils/ui/dialogs';
import Cancel from '~icons/tabler/x';
import Users from '~icons/tabler/users';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

const USERS_ROUTE = '/user';

export function openDefaultCredentialsWarning() {
	return confirm({
		title: m.auth_default_credentials_title({ locale: i18n.languageTag }),
		message: m.auth_default_credentials_message({ locale: i18n.languageTag }),
		labels: {
			cancel: {
				label: m.auth_default_credentials_skip({ locale: i18n.languageTag }),
				icon: Cancel
			},
			confirm: {
				label: m.auth_default_credentials_go_to_users({ locale: i18n.languageTag }),
				icon: Users
			}
		},
		onCancel: () => {},
		onConfirm: () => {
			void goto(USERS_ROUTE);
		}
	});
}
