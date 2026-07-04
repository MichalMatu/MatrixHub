import type { Component } from 'svelte';
import Temperature from '~icons/tabler/temperature';
import Droplet from '~icons/tabler/droplet';
import Cloud from '~icons/tabler/cloud';
import { CHART_COLORS } from '$lib/constants';

export type SensorType = 'temperature' | 'humidity' | 'co2';

export interface ChartConfig {
	icon: Component;
	color: string;
	unit: string;
	decimals: number;
	shadowColor: 'error' | 'info' | 'success' | 'warning' | 'primary';
	yRange?: [number, number];
	/** Minimum Y-axis padding in data units (e.g. 5 for ±5°C). Used only with auto range. */
	minPadding?: number;
	/** Minimum Y-axis value (floor). Auto-range won't go below this. */
	yFloor?: number;
}

export const CHART_CONFIGS: Record<SensorType, ChartConfig> = {
	temperature: {
		icon: Temperature,
		color: CHART_COLORS.temperature,
		unit: '°C',
		decimals: 1,
		shadowColor: 'error',
		minPadding: 5,
		yFloor: 0
	},
	humidity: {
		icon: Droplet,
		color: CHART_COLORS.humidity,
		unit: '%',
		decimals: 1,
		shadowColor: 'info',
		yRange: [0, 100]
	},
	co2: {
		icon: Cloud,
		color: CHART_COLORS.co2,
		unit: 'ppm',
		decimals: 0,
		shadowColor: 'success',
		minPadding: 100
	}
};
