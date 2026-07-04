import { describe, it, expect } from 'vitest';
import { CHART_CONFIGS } from './chartConfigs';

describe('CHART_CONFIGS', () => {
	it('should have configuration for all sensor types', () => {
		expect(CHART_CONFIGS).toHaveProperty('temperature');
		expect(CHART_CONFIGS).toHaveProperty('humidity');
		expect(CHART_CONFIGS).toHaveProperty('co2');
	});

	it('should have correct units', () => {
		expect(CHART_CONFIGS.temperature.unit).toBe('°C');
		expect(CHART_CONFIGS.humidity.unit).toBe('%');
		expect(CHART_CONFIGS.co2.unit).toBe('ppm');
	});

	it('should have correct decimals', () => {
		expect(CHART_CONFIGS.temperature.decimals).toBe(1);
		expect(CHART_CONFIGS.humidity.decimals).toBe(1);
		expect(CHART_CONFIGS.co2.decimals).toBe(0);
	});

	it('should have valid shadow colors', () => {
		const validColors = ['error', 'info', 'warning', 'success', 'primary'];
		expect(validColors).toContain(CHART_CONFIGS.temperature.shadowColor);
		expect(validColors).toContain(CHART_CONFIGS.humidity.shadowColor);
		expect(validColors).toContain(CHART_CONFIGS.co2.shadowColor);
	});
});
