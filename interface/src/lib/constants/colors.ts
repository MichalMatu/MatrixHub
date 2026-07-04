/**
 * Color constants for charts, shadows, and UI elements.
 * Single source of truth for all color values across the application.
 */

/**
 * Chart colors for sensor data visualization
 */
export const CHART_COLORS = {
	/** Temperature chart - red */
	temperature: '#ef4444',
	/** Humidity chart - blue */
	humidity: '#3b82f6',
	/** CO2 chart - green */
	co2: '#22c55e',
	/** Motion/WiFi sensing - amber */
	motion: '#f59e0b',
	/** Inactive/no motion - blue */
	inactive: '#3b82f6'
} as const;
