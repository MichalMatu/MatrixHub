export interface PowerConfig {
	sleep_enabled: boolean;
	inactivity_timeout_ms: number;
	grace_after_boot_ms: number;
	wake_timer_enabled: boolean;
	wake_button_enabled: boolean;
	wake_touch_enabled: boolean;
	wake_interval_ms: number;
	timer_wake_awake_ms: number;
	button_wake_awake_ms: number;
	wake_touch_gpio: number;
	wake_touch_threshold: number;
}

export interface PowerStatus extends PowerConfig {
	wake_reason: string;
	wake_cause_raw: number;
	wake_gpio_mask: string;
	wake_ext1_mask: string;
	sleep_requested: boolean;
	sleep_eta_ms: number;
	wake_awake_window_ms: number;
	wake_awake_eta_ms: number;
	last_activity_ms: number;
	thermal_state: 'normal' | 'soft_throttle' | 'hard_throttle' | 'critical' | 'unknown';
	thermal_temp_c: number | null;
	thermal_cpu_mhz: number;
	thermal_throttled: boolean;
	thermal_soft_c: number;
	thermal_hard_c: number;
	thermal_critical_c: number;
	uptime_ms: number;
}
