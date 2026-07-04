/**
 * Air Mouse status and settings types
 */
export interface AirMouseImuData {
	gx: number;
	gy: number;
	gz: number;
	ax: number;
	ay: number;
	az: number;
}

// Backend no longer exposes a transport selector here. AirMouse ownership is
// derived from movement/click/jiggler toggles, and runtime remains USB-backed.
export interface AirMouseStatus {
	movement_enabled: boolean;
	click_enabled: boolean;
	running: boolean;
	calibrating: boolean;
	sensitivity_x: number;
	sensitivity_y: number;
	deadzone: number;
	acceleration_enabled: boolean;
	acceleration_factor: number;
	tap_threshold_g: number;
	click_debounce_ms: number;
	double_click_window_ms: number;
	click_source: number;
	single_click_action: number;
	double_click_action: number;
	triple_click_action: number;
	single_click_script: string;
	double_click_script: string;
	triple_click_script: string;
	euro_min_cutoff: number;
	euro_beta: number;
	euro_d_cutoff: number;
	gyro_offset_x: number;
	gyro_offset_y: number;
	gyro_offset_z: number;
	last_delta_g: number;
	jiggler: {
		mode: number;
		interval: number;
		move_distance: number;
		random_interval: boolean;
	};
	imu: AirMouseImuData;
}

export interface AirMouseConfig {
	movement_enabled?: boolean;
	click_enabled?: boolean;
	sensitivity_x?: number;
	sensitivity_y?: number;
	deadzone?: number;
	acceleration_enabled?: boolean;
	acceleration_factor?: number;
	tap_threshold_g?: number;
	click_debounce_ms?: number;
	double_click_window_ms?: number;
	click_source?: number;
	single_click_action?: number;
	double_click_action?: number;
	triple_click_action?: number;
	single_click_script?: string;
	double_click_script?: string;
	triple_click_script?: string;
	euro_min_cutoff?: number;
	euro_beta?: number;
	euro_d_cutoff?: number;
	jiggler?: {
		mode: number;
		interval: number;
		move_distance: number;
		random_interval: boolean;
	};
}
