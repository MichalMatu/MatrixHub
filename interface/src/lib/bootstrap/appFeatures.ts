import { z } from 'zod';

const AppFeaturesSchema = z.object({
	ntp: z.boolean().optional(),
	sleep: z.boolean().optional(),
	file_manager: z.boolean().optional(),
	firmware_version: z.string().optional(),
	firmware_name: z.string().optional(),
	firmware_built_target: z.string().optional()
});

export type AppFeatures = {
	ntp: boolean;
	sleep: boolean;
	file_manager?: boolean;
	firmware_version?: string;
	firmware_name?: string;
	firmware_built_target?: string;
};

export const DEFAULT_APP_FEATURES: AppFeatures = {
	ntp: true,
	sleep: true
};

export function resolveAppFeatures(payload: unknown): AppFeatures {
	const parsed = AppFeaturesSchema.safeParse(payload);
	if (!parsed.success) {
		return { ...DEFAULT_APP_FEATURES };
	}

	return {
		...DEFAULT_APP_FEATURES,
		...parsed.data
	};
}

export async function fetchAppFeatures(
	fetchImpl: typeof fetch,
	options: { timeoutMs?: number } = {}
): Promise<AppFeatures> {
	const timeoutMs = options.timeoutMs ?? 5000;
	const controller = new AbortController();
	const timeoutId = setTimeout(() => controller.abort(), timeoutMs);

	try {
		const response = await fetchImpl('/rest/features', {
			headers: { Accept: 'application/json' },
			signal: controller.signal
		});

		if (!response.ok) {
			return { ...DEFAULT_APP_FEATURES };
		}

		const payload = await response.json();
		return resolveAppFeatures(payload);
	} catch {
		return { ...DEFAULT_APP_FEATURES };
	} finally {
		clearTimeout(timeoutId);
	}
}
