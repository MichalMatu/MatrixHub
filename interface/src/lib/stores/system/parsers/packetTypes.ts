import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';
import type { SystemStatus } from '$lib/types/system/systemStatus';
import type { SystemEvent } from '../types';

export type SystemEventBusLike = {
	set: (value: SystemEvent | null) => void;
};

export interface StoreMappers {
	systemEvents: SystemEventBusLike;
	updateStatus: (updater: (s: SystemStatus) => SystemStatus) => void;
	updateMacros?: (m: ScriptStatus) => void;
	telemetry?: unknown;
}
