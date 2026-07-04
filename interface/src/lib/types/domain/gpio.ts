export type GpioMode = 'disabled' | 'input' | 'output';
export type GpioPull = 'none' | 'up' | 'down';

export interface GpioPinDefinition {
	id: string;
	name: string;
	pin: number;
	allowed: boolean;
	input: boolean;
	output: boolean;
	pull_up: boolean;
	pull_down: boolean;
	reason?: string;
}

export interface GpioChannelConfig {
	id: string;
	name: string;
	pin: number;
	mode: GpioMode;
	pull: GpioPull;
	inverted: boolean;
	debounce_ms: number;
	initial_output: boolean;
}

export interface GpioConfig {
	channels: GpioChannelConfig[];
}

export interface GpioChannelStatus {
	id: string;
	name: string;
	pin: number;
	mode: GpioMode;
	configured: boolean;
	raw: boolean;
	logical: boolean;
	stable: boolean;
	sampled_at: number;
	changed_at: number;
}

export interface GpioStatus {
	channels: GpioChannelStatus[];
}

export interface GpioOutputRequest {
	id: string;
	value: boolean;
}
