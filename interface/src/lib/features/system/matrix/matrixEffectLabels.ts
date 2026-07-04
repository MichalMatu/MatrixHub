import * as m from '$lib/paraglide/messages.js';
import {
	MATRIX_EFFECT_ENGINE_LEGACY,
	MATRIX_EFFECT_ENGINE_NATIVE_3D,
	type MatrixEffectEngine
} from './matrixModel';

const effectLabelResolvers: Record<number, (args: { locale: string }) => string> = {
	0: m.matrix_eff_static,
	1: m.matrix_eff_blink,
	2: m.matrix_eff_breath,
	3: m.matrix_eff_color_wipe,
	4: m.matrix_eff_color_wipe_inv,
	5: m.matrix_eff_color_wipe_rev,
	6: m.matrix_eff_color_wipe_rev_inv,
	7: m.matrix_eff_color_wipe_random,
	8: m.matrix_eff_random_color,
	9: m.matrix_eff_single_dynamic,
	10: m.matrix_eff_multi_dynamic,
	11: m.matrix_eff_rainbow,
	12: m.matrix_eff_rainbow_cycle,
	13: m.matrix_eff_scan,
	14: m.matrix_eff_dual_scan,
	15: m.matrix_eff_fade,
	16: m.matrix_eff_theater_chase,
	17: m.matrix_eff_theater_chase_rainbow,
	18: m.matrix_eff_running_lights,
	19: m.matrix_eff_twinkle,
	20: m.matrix_eff_twinkle_random,
	21: m.matrix_eff_twinkle_fade,
	22: m.matrix_eff_twinkle_fade_random,
	23: m.matrix_eff_sparkle,
	24: m.matrix_eff_flash_sparkle,
	25: m.matrix_eff_hyper_sparkle,
	26: m.matrix_eff_strobe,
	27: m.matrix_eff_strobe_rainbow,
	28: m.matrix_eff_multi_strobe,
	29: m.matrix_eff_blink_rainbow,
	30: m.matrix_eff_chase_white,
	31: m.matrix_eff_chase_color,
	32: m.matrix_eff_chase_random,
	33: m.matrix_eff_chase_rainbow,
	34: m.matrix_eff_chase_flash,
	35: m.matrix_eff_chase_flash_random,
	36: m.matrix_eff_chase_rainbow_white,
	37: m.matrix_eff_chase_blackout,
	38: m.matrix_eff_chase_blackout_rainbow,
	39: m.matrix_eff_color_sweep_random,
	40: m.matrix_eff_running_color,
	41: m.matrix_eff_running_red_blue,
	42: m.matrix_eff_running_random,
	43: m.matrix_eff_larson_scanner,
	44: m.matrix_eff_comet,
	45: m.matrix_eff_fireworks,
	46: m.matrix_eff_fireworks_random,
	47: m.matrix_eff_merry_christmas,
	48: m.matrix_eff_fire_flicker,
	49: m.matrix_eff_fire_flicker_soft,
	50: m.matrix_eff_fire_flicker_intense,
	51: m.matrix_eff_circus_combustus,
	52: m.matrix_eff_halloween,
	53: m.matrix_eff_bicolor_chase,
	54: m.matrix_eff_tricolor_chase,
	55: m.matrix_eff_twinklefox,
	56: m.matrix_eff_rain,
	57: m.matrix_eff_block_dissolve,
	58: m.matrix_eff_icu,
	59: m.matrix_eff_dual_larson,
	60: m.matrix_eff_running_random2,
	61: m.matrix_eff_filler_up,
	62: m.matrix_eff_rainbow_larson,
	63: m.matrix_eff_rainbow_fireworks,
	64: m.matrix_eff_trifade,
	65: m.matrix_eff_heartbeat,
	66: m.matrix_eff_bits,
	67: m.matrix_eff_multi_comet,
	68: m.matrix_eff_popcorn,
	69: m.matrix_eff_oscillator
};

const native3dEffectLabelResolvers: Record<number, (args: { locale: string }) => string> = {
	0: m.matrix_eff_3d_gyro_cube,
	1: m.matrix_eff_3d_gravity_particles,
	2: m.matrix_eff_3d_depth_tunnel,
	3: m.matrix_eff_3d_liquid_wave,
	4: m.matrix_eff_3d_oil_sheen,
	5: m.matrix_eff_3d_aurora_curtain,
	6: m.matrix_eff_3d_plasma_cloud,
	7: m.matrix_eff_3d_orbital_rings,
	8: m.matrix_eff_3d_tilt_vortex,
	9: m.matrix_eff_3d_comet_field,
	10: m.matrix_eff_3d_lava_lamp,
	11: m.matrix_eff_3d_prism_sweep,
	12: m.matrix_eff_3d_rain_glass,
	13: m.matrix_eff_3d_breathing_terrain,
	14: m.matrix_eff_3d_palette_wave
};

export function getMatrixEffectName(
	effectId: number,
	locale: string,
	engine: MatrixEffectEngine = MATRIX_EFFECT_ENGINE_LEGACY
): string {
	if (engine === MATRIX_EFFECT_ENGINE_NATIVE_3D) {
		return native3dEffectLabelResolvers[effectId]?.({ locale }) ?? `3D Effect ${effectId}`;
	}
	return effectLabelResolvers[effectId]?.({ locale }) ?? `Effect ${effectId}`;
}
