import { describe, expect, it } from 'vitest';
import {
	DASHBOARD_SPARKLINE_SCALE_CONFIGS,
	resolveDashboardSparklineDomain
} from './dashboardSparklineScale';

describe('resolveDashboardSparklineDomain', () => {
	it('keeps narrow co2 changes inside a stable rounded viewing window', () => {
		const domain = resolveDashboardSparklineDomain(
			[612, 618, 620],
			DASHBOARD_SPARKLINE_SCALE_CONFIGS.co2
		);

		expect(domain).not.toBeNull();
		expect(domain!.min).toBeLessThanOrEqual(612);
		expect(domain!.max).toBeGreaterThanOrEqual(620);
		expect(domain!.min % 10).toBe(0);
		expect(domain!.max % 10).toBe(0);
		expect(domain!.max - domain!.min).toBeGreaterThanOrEqual(160);
	});

	it('keeps narrow temperature changes from stretching across the full card height', () => {
		const domain = resolveDashboardSparklineDomain(
			[22.1, 22.2, 22.3],
			DASHBOARD_SPARKLINE_SCALE_CONFIGS.temp
		);

		expect(domain).not.toBeNull();
		expect(domain!.min).toBeLessThanOrEqual(22.1);
		expect(domain!.max).toBeGreaterThanOrEqual(22.3);
		expect(domain!.max - domain!.min).toBeGreaterThanOrEqual(2);
		expect(domain!.max - domain!.min).toBeLessThan(3);
	});

	it('uses a tighter dynamic humidity window so small changes remain visible', () => {
		const domain = resolveDashboardSparklineDomain(
			[54.8, 55.1, 55.4],
			DASHBOARD_SPARKLINE_SCALE_CONFIGS.humid
		);

		expect(domain).not.toBeNull();
		expect(domain!.min).toBeGreaterThan(0);
		expect(domain!.max).toBeLessThan(100);
		expect(domain!.max - domain!.min).toBeGreaterThanOrEqual(2);
		expect(domain!.max - domain!.min).toBeLessThan(4);
	});

	it('keeps humidity readable near the upper bound by shifting the scale down', () => {
		const domain = resolveDashboardSparklineDomain(
			[99.4, 99.7, 99.9],
			DASHBOARD_SPARKLINE_SCALE_CONFIGS.humid
		);

		expect(domain).not.toBeNull();
		expect(domain!.max).toBe(100);
		expect(domain!.min).toBeLessThan(98);
		expect(domain!.max - domain!.min).toBeGreaterThanOrEqual(2);
	});
});
