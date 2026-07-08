import type { Component } from 'svelte';
import * as m from '$lib/paraglide/messages.js';
import { menuIcons } from './menuIcons';

export type NavigationContext = {
	ntpEnabled: boolean;
	canManage: boolean;
	isStaConnected: boolean;
};

type FeatureLabelResolver = (options: { locale?: string }) => string;

type NavigationLeafId =
	| 'alarms'
	| 'telegram'
	| 'pushover'
	| 'webhook'
	| 'heartbeat'
	| 'udp'
	| 'charts'
	| 'wifi_sta'
	| 'wifi_ap'
	| 'shelly'
	| 'gpio'
	| 'wifi_sensing'
	| 'wifi_csi'
	| 'bluetooth'
	| 'airmouse'
	| 'mouse_jiggler'
	| 'keyboard'
	| 'macros'
	| 'usb_terminal'
	| 'status'
	| 'logs'
	| 'time'
	| 'compensation'
	| 'imu'
	| 'power'
	| 'matrix_led'
	| 'users'
	| 'files'
	| 'styles'
	| 'help';

type NavigationGroupId = 'notifications' | 'wifi' | 'usb_features' | 'system';

export type FeatureLinkId = NavigationLeafId;
type FeatureNodeId = NavigationLeafId | NavigationGroupId;

type NavigationGuard = 'admin' | 'staConnected' | 'ntpEnabled';

export type FeatureLink = {
	href: string;
	label: string;
	adminOnly?: boolean;
};

export type FeatureLinkGroup = {
	title: string;
	links: FeatureLink[];
};

type FeatureLeafDefinition = {
	type: 'leaf';
	id: NavigationLeafId;
	href: string;
	activePathPrefixes?: string[];
	icon: Component;
	label: FeatureLabelResolver;
	guards?: NavigationGuard[];
};

type FeatureGroupDefinition = {
	type: 'group';
	id: NavigationGroupId;
	icon: Component;
	label: FeatureLabelResolver;
	guards?: NavigationGuard[];
	children: NavigationLeafId[];
};

type FeatureNodeDefinition = FeatureLeafDefinition | FeatureGroupDefinition;

type HelpLinkGroupDefinition = {
	title: FeatureLabelResolver;
	links: FeatureLinkId[];
};

const registry = [
	{
		type: 'leaf',
		id: 'alarms',
		href: '/alarms',
		icon: menuIcons.Bell,
		label: m.menu_alarms
	},
	{
		type: 'group',
		id: 'notifications',
		icon: menuIcons.Message,
		label: m.menu_notifications,
		guards: ['admin'],
		children: ['telegram', 'pushover', 'webhook', 'heartbeat', 'udp']
	},
	{
		type: 'leaf',
		id: 'telegram',
		href: '/settings/notifications/telegram',
		icon: menuIcons.Message,
		label: m.menu_telegram,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'pushover',
		href: '/settings/notifications/pushover',
		icon: menuIcons.Message,
		label: m.menu_pushover,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'webhook',
		href: '/settings/notifications/webhook',
		icon: menuIcons.Webhook,
		label: m.menu_webhook,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'heartbeat',
		href: '/settings/notifications/heartbeat',
		icon: menuIcons.Heartbeat,
		label: m.menu_heartbeat,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'udp',
		href: '/settings/integrations/udp',
		icon: menuIcons.NetworkSend,
		label: m.menu_udp,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'charts',
		href: '/charts',
		icon: menuIcons.ChartLine,
		label: m.menu_charts
	},
	{
		type: 'group',
		id: 'wifi',
		icon: menuIcons.Wifi,
		label: m.menu_wifi,
		children: ['wifi_sta', 'wifi_ap', 'shelly', 'wifi_sensing', 'wifi_csi']
	},
	{
		type: 'leaf',
		id: 'wifi_sta',
		href: '/wifi/sta',
		icon: menuIcons.Router,
		label: m.menu_wifi_sta
	},
	{
		type: 'leaf',
		id: 'wifi_ap',
		href: '/wifi/ap',
		icon: menuIcons.AP,
		label: m.menu_wifi_ap
	},
	{
		type: 'leaf',
		id: 'shelly',
		href: '/shelly',
		icon: menuIcons.Plug,
		label: m.menu_shelly
	},
	{
		type: 'leaf',
		id: 'wifi_sensing',
		href: '/wifisensing',
		icon: menuIcons.Wifi,
		label: m.menu_wifisensing
	},
	{
		type: 'leaf',
		id: 'wifi_csi',
		href: '/wifisensing/csi',
		icon: menuIcons.ChartLine,
		label: m.menu_wifi_csi,
		guards: ['staConnected']
	},
	{
		type: 'leaf',
		id: 'bluetooth',
		href: '/bluetooth',
		icon: menuIcons.Bluetooth,
		label: m.menu_bluetooth
	},
	{
		type: 'leaf',
		id: 'gpio',
		href: '/gpio',
		icon: menuIcons.Cpu2,
		label: m.menu_gpio,
		guards: ['admin']
	},
	{
		type: 'group',
		id: 'usb_features',
		icon: menuIcons.Plug,
		label: m.menu_usb_features,
		children: ['airmouse', 'mouse_jiggler', 'keyboard', 'macros', 'usb_terminal']
	},
	{
		type: 'leaf',
		id: 'airmouse',
		href: '/usb-features/airmouse',
		icon: menuIcons.Mouse,
		label: m.menu_airmouse,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'mouse_jiggler',
		href: '/usb-features/jiggler',
		icon: menuIcons.Target,
		label: m.menu_mouse_jiggler,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'keyboard',
		href: '/usb-features/keyboard',
		icon: menuIcons.Keyboard,
		label: m.menu_keyboard,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'macros',
		href: '/usb-features/macros',
		icon: menuIcons.Terminal,
		label: m.menu_macros,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'usb_terminal',
		href: '/usb-features/terminal',
		icon: menuIcons.Terminal,
		label: m.menu_usb_terminal,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'matrix_led',
		href: '/system/matrix',
		activePathPrefixes: ['/system/matrix'],
		icon: menuIcons.GridDots,
		label: m.menu_matrix_led
	},
	{
		type: 'group',
		id: 'system',
		icon: menuIcons.Settings,
		label: m.menu_system,
		children: ['status', 'logs', 'time', 'compensation', 'imu', 'power', 'users', 'files', 'styles']
	},
	{
		type: 'leaf',
		id: 'status',
		href: '/system/status',
		icon: menuIcons.Health,
		label: m.menu_status
	},
	{
		type: 'leaf',
		id: 'logs',
		href: '/logs',
		icon: menuIcons.FileText,
		label: m.menu_logs
	},
	{
		type: 'leaf',
		id: 'time',
		href: '/connections/ntp',
		icon: menuIcons.Clock,
		label: m.menu_time,
		guards: ['ntpEnabled']
	},
	{
		type: 'leaf',
		id: 'compensation',
		href: '/settings/sensors/compensation',
		icon: menuIcons.Thermometer,
		label: m.menu_compensation,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'imu',
		href: '/settings/sensors/imu',
		icon: menuIcons.Activity,
		label: m.menu_imu,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'power',
		href: '/system/power',
		icon: menuIcons.Power,
		label: m.menu_power,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'users',
		href: '/user',
		icon: menuIcons.Users,
		label: m.menu_users,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'files',
		href: '/system/file-manager',
		icon: menuIcons.Folder,
		label: m.menu_files,
		guards: ['admin']
	},
	{
		type: 'leaf',
		id: 'styles',
		href: '/system/styles',
		icon: menuIcons.Settings,
		label: m.menu_styles
	},
	{
		type: 'leaf',
		id: 'help',
		href: '/system/help',
		icon: menuIcons.HelpCircle,
		label: m.menu_help
	}
] satisfies FeatureNodeDefinition[];

const helpGroupDefinitions = {
	alerts: {
		title: m.help_alarm_flow_title,
		links: ['alarms', 'charts', 'telegram', 'pushover', 'webhook']
	},
	troubleshooting: {
		title: m.help_troubleshooting_title,
		links: ['wifi_sta', 'wifi_ap', 'status', 'logs']
	},
	setup: {
		title: m.help_quick_links_setup,
		links: ['wifi_sta', 'wifi_ap', 'time', 'alarms']
	},
	notify: {
		title: m.help_quick_links_notify,
		links: ['telegram', 'pushover', 'webhook', 'heartbeat', 'udp']
	},
	diagnostics: {
		title: m.help_quick_links_diagnostics,
		links: ['status', 'logs', 'charts', 'compensation', 'matrix_led']
	},
	integrations: {
		title: m.help_section_integrations_title,
		links: ['shelly', 'bluetooth', 'gpio', 'wifi_sensing', 'airmouse']
	},
	system: {
		title: m.help_section_system_title,
		links: ['styles', 'matrix_led', 'files', 'users', 'power']
	}
} satisfies Record<string, HelpLinkGroupDefinition>;

export const shellyHelpLinkIds: FeatureLinkId[] = ['alarms', 'help'];
export const wifiSensingHelpLinkIds: FeatureLinkId[] = ['wifi_csi', 'status', 'help'];
export const powerHelpLinkIds: FeatureLinkId[] = ['status', 'help'];

function getNode(id: FeatureNodeId): FeatureNodeDefinition {
	const entry = registry.find((candidate) => candidate.id === id);
	if (!entry) {
		throw new Error(`Unknown navigation node: ${id}`);
	}
	return entry;
}

function getLeaf(id: FeatureLinkId): FeatureLeafDefinition {
	const entry = getNode(id);
	if (entry.type !== 'leaf') {
		throw new Error(`Expected navigation leaf: ${id}`);
	}
	return entry;
}

function resolveLabel(label: FeatureLabelResolver, locale?: string) {
	return label({ locale });
}

function resolveGuardReason(
	guard: NavigationGuard,
	context: NavigationContext,
	locale?: string
): string | null {
	switch (guard) {
		case 'admin':
			return context.canManage ? null : m.menu_locked_admin({ locale });
		case 'staConnected':
			return context.isStaConnected ? null : m.menu_locked_sta_connection({ locale });
		case 'ntpEnabled':
			return context.ntpEnabled ? null : m.menu_locked_feature_unavailable({ locale });
	}
}

function resolveDisabledReason(
	guards: NavigationGuard[] | undefined,
	context: NavigationContext,
	locale?: string
) {
	for (const guard of guards ?? []) {
		const disabledReason = resolveGuardReason(guard, context, locale);
		if (disabledReason) {
			return disabledReason;
		}
	}
	return null;
}

function createFeatureLink(id: FeatureLinkId, locale?: string): FeatureLink {
	const definition = getLeaf(id);
	return {
		href: definition.href,
		label: resolveLabel(definition.label, locale),
		adminOnly: definition.guards?.includes('admin')
	};
}

export function createFeatureLinks(ids: FeatureLinkId[], locale?: string): FeatureLink[] {
	return ids.map((id) => createFeatureLink(id, locale));
}

function createHelpLinkGroup(groupId: keyof typeof helpGroupDefinitions, locale?: string) {
	const definition = helpGroupDefinitions[groupId];
	return {
		title: resolveLabel(definition.title, locale),
		links: createFeatureLinks(definition.links, locale)
	} satisfies FeatureLinkGroup;
}

export function createHelpLinkGroups(
	groupIds: Array<keyof typeof helpGroupDefinitions>,
	locale?: string
) {
	return groupIds.map((groupId) => createHelpLinkGroup(groupId, locale));
}

export function resolveFeatureTitle(pathname: string, locale?: string): string | null {
	const leaf = registry.find(
		(entry) => entry.type === 'leaf' && isFeaturePathActive(entry, pathname)
	);
	if (!leaf || leaf.type !== 'leaf') {
		return null;
	}
	return resolveLabel(leaf.label, locale);
}

function isFeaturePathActive(entry: FeatureLeafDefinition, pathname: string) {
	return (
		entry.href === pathname ||
		(entry.activePathPrefixes ?? []).some(
			(prefix) => pathname === prefix || pathname.startsWith(`${prefix}/`)
		)
	);
}

export function createNavigationTree(
	context: NavigationContext,
	locale?: string,
	currentPath?: string
) {
	return registry
		.filter((entry) => entry.type === 'group' || entry.type === 'leaf')
		.filter((entry) => entry.type === 'group' || isTopLevelLeaf(entry.id))
		.map((entry) => {
			if (entry.type === 'leaf') {
				return {
					title: resolveLabel(entry.label, locale),
					icon: entry.icon,
					href: entry.href,
					feature: true,
					active: currentPath ? isFeaturePathActive(entry, currentPath) : false,
					disabledReason: resolveDisabledReason(entry.guards, context, locale) ?? undefined
				};
			}

			const submenu = entry.children.map((childId) => {
				const child = getLeaf(childId);
				return {
					title: resolveLabel(child.label, locale),
					icon: child.icon,
					href: child.href,
					feature: true,
					active: currentPath ? isFeaturePathActive(child, currentPath) : false,
					disabledReason: resolveDisabledReason(child.guards, context, locale) ?? undefined
				};
			});

			return {
				title: resolveLabel(entry.label, locale),
				icon: entry.icon,
				feature: true,
				active: submenu.some((item) => item.active),
				disabledReason: resolveDisabledReason(entry.guards, context, locale) ?? undefined,
				submenu
			};
		});
}

function isTopLevelLeaf(id: NavigationLeafId) {
	return !registry.some(
		(entry) => entry.type === 'group' && entry.children.some((childId) => childId === id)
	);
}
