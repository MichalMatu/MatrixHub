/**
 * Power Management preset styles
 */

export const powerManagementStyles = {
	shell: 'surface-card',
	section: 'surface-panel',
	infoPill:
		'inline-flex items-center gap-2 rounded-full border border-base-content/10 bg-base-100/70 px-3 py-1 text-xs font-medium text-base-content/70',
	badge: 'badge badge-outline uppercase tracking-wide text-xs',
	badgeWithIcon: 'badge badge-outline flex items-center gap-2 text-xs uppercase tracking-wide',
	summaryCard: 'detail-tile items-start gap-4',
	summaryIcon: 'detail-tile-icon'
} as const;
