import { z } from 'zod';
import type { SystemInformation } from '$lib/types/system/system';
import { WifiNetworkSchema } from '$lib/features/wifi/wifiValidation';

export { WifiNetworkSchema };

export const SystemInformationSchema = z.object({
	esp_platform: z.string(),
	firmware_version: z.string(),
	firmware_name: z.string().optional(),
	firmware_built_target: z.string().optional(),
	cpu_freq_mhz: z.number(),
	cpu_type: z.string(),
	cpu_rev: z.number(),
	cpu_cores: z.number(),
	sketch_size: z.number(),
	free_sketch_space: z.number(),
	sdk_version: z.string(),
	arduino_version: z.string(),
	flash_chip_size: z.number(),
	flash_chip_speed: z.number(),
	cpu_reset_reason: z.number(),
	max_alloc_heap: z.number(),
	psram_size: z.number(),
	free_psram: z.number(),
	used_psram: z.number(),
	free_heap: z.number(),
	used_heap: z.number(),
	total_heap: z.number(),
	min_free_heap: z.number(),
	core_temp: z.number(),
	fs_total: z.number(),
	fs_used: z.number(),
	lp_sram_used: z.number().optional(),
	lp_sram_free: z.number().optional(),
	lp_sram_total: z.number().optional(),
	uptime: z.number(),
	mac_address: z.string().optional(),
	compile_date: z.string().optional(),
	compile_time: z.string().optional()
}) satisfies z.ZodType<SystemInformation>;

export const WifiRecoveryResponseSchema = z.object({
	success: z.boolean(),
	accepted: z.boolean(),
	connected: z.boolean(),
	ip: z.string().optional(),
	rssi: z.number().optional()
});

export const BleStatusSchema = z.object({
	enabled: z.boolean(),
	running: z.boolean(),
	scanner_active: z.boolean().optional(),
	metrics: z
		.object({
			adv_total: z.number(),
			valid_packets: z.number(),
			parser_errors: z.number(),
			cache_drops: z.number(),
			mutex_timeouts: z.number(),
			scanner_running: z.boolean()
		})
		.optional(),
	devices: z
		.array(
			z.object({
				mac: z.string(),
				temp: z.number(),
				humid: z.number(),
				batt: z.number(),
				rssi: z.number(),
				last_seen: z.number(),
				saved: z.boolean().optional()
			})
		)
		.optional()
});

export const BleSettingsSchema = z.object({
	enabled: z.boolean(),
	sensors: z
		.array(
			z.object({
				mac: z.string(),
				alias: z.string()
			})
		)
		.optional()
});

const MatrixColorSchema = z.number().int().min(0).max(0xffffff);
const MatrixCustomIconSchema = z
	.array(MatrixColorSchema)
	.refine((pixels) => pixels.length === 0 || pixels.length === 64, {
		message: 'Matrix custom icon slots must be empty or contain 64 pixels'
	});

export const MatrixSettingsSchema = z.object({
	brightness: z.number().int().min(2).max(255),
	alarm_mode: z.number().int().min(0).max(2),
	rotation: z.number().int().min(0).max(3),
	auto_rotate: z.boolean(),
	effect_enabled: z.boolean(),
	effect_engine: z.number().int().min(0).max(1),
	effect_mode: z.number().int().min(0).max(69),
	effect_speed: z
		.number()
		.int()
		.min(50)
		.max(24 * 60 * 60 * 1000),
	effect_color: MatrixColorSchema,
	effect_color_2: MatrixColorSchema,
	effect_color_3: MatrixColorSchema,
	effect_reactivity_provider: z.number().int().min(0).max(1),
	effect_reactivity_gain: z.number().int().min(0).max(200),
	background_mode: z.number().int().min(0).max(1),
	data_visualization_enabled: z.boolean(),
	data_visualization_source: z.number().int().min(0).max(3),
	data_visualization_metric: z.number().int().min(0).max(5),
	data_visualization_mode: z.number().int().min(0).max(6),
	data_visualization_min: z.number(),
	data_visualization_max: z.number(),
	data_visualization_color_min: MatrixColorSchema,
	data_visualization_color_mid: MatrixColorSchema,
	data_visualization_color_max: MatrixColorSchema,
	data_visualization_brightness_min: z.number().int().min(0).max(255),
	data_visualization_brightness_max: z.number().int().min(0).max(255),
	data_visualization_smoothing: z.number().int().min(0).max(100),
	data_visualization_stale_behavior: z.number().int().min(0).max(2),
	data_visualization_device_id: z.string().max(17),
	custom_icons: z.array(MatrixCustomIconSchema).length(3).optional(),
	menu_enabled: z.boolean(),
	menu_text_color: MatrixColorSchema,
	menu_scroll_speed: z.number().int().min(20).max(120)
});
