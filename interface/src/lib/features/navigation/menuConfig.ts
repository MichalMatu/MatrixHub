import type { Component } from 'svelte';
import { createNavigationTree, type NavigationContext } from './featureRegistry';

export type MenuItem = {
	title: string;
	icon: Component;
	href?: string;
	feature: boolean | (() => boolean);
	active?: boolean;
	disabledReason?: string | null;
	submenu?: SubMenuItem[];
};

export type SubMenuItem = {
	title: string;
	icon: Component;
	href: string;
	feature: boolean | (() => boolean);
	active?: boolean;
	disabledReason?: string | null;
};

export function createMenuStructure(
	options: NavigationContext,
	locale?: string,
	currentPath?: string
): MenuItem[] {
	return createNavigationTree(options, locale, currentPath);
}
