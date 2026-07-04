import { useAirMouseManagement } from './useAirMouseManagement.svelte';

export { useAirMouseManagement };
export type AirMouseState = ReturnType<typeof useAirMouseManagement>;
