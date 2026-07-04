import { describe, expect, it } from 'vitest';

describe('design system barrel exports', () => {
	it('exposes tokens, variant helpers, presets, and utilities', async () => {
		const designSystem = await import('../design-system');

		expect(designSystem.icon.wrapper.panel).toContain('rounded-2xl');
		expect(designSystem.semantic.page.container).toContain('space-y');

		expect(designSystem.getButtonClasses('ghost', 'sm')).toContain('btn-ghost');
		expect(designSystem.getButtonClasses('icon', 'lg')).toContain('btn-circle');
		// eslint-disable-next-line @typescript-eslint/no-explicit-any
		expect(designSystem.getButtonClasses('unknown' as any, 'md')).toContain('btn-primary');

		expect(designSystem.getInputClasses(true, false, 'xs')).toContain('border-error');
		expect(designSystem.getInputClasses(false, true, 'lg')).toContain('max-w-xs');
		expect(designSystem.getInputClasses(true, true, 'sm')).toContain('border-error');
		expect(designSystem.cardVariants.interactive).toContain('hover:shadow-md');
		expect(designSystem.dialogVariants.md).toContain('max-w-md');
		expect(designSystem.formVariants.actions).toContain('justify-end');

		expect(designSystem.surfaceCard('px-4')).toContain('surface-card');
		expect(designSystem.surfacePanel('shadow')).toContain('surface-panel');
		expect(designSystem.detailTile('gap-2')).toContain('detail-tile');
		expect(designSystem.miniCard('gap-1')).toContain('mini-card');

		expect(designSystem.fileManagerStyles.headerBadge).toContain('badge');
		expect(designSystem.powerManagementStyles.badgeWithIcon).toContain('badge-outline');
	});
});
