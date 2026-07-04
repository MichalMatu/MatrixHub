/**
 * Lib utilities barrel export
 * Import all utilities from '$lib/utils'
 */

// Formatters
export { formatBytes, formatDuration, formatTemperature, escapeHtml } from './format/formatters';

// Validators
export { isValidTelegramBotToken, isValidHttpUrl } from './validation/validators';

// API Client
export { createApiClient, type ApiClient, type ApiClientOptions, ApiError } from './api/apiClient';

// Request / fetch error helpers
export * from './api/requestErrors';

// Re-export existing utils
export * from './charts';
export * from './validation/sensorClassification';
export * from './format/timeFormat';
export { normalizeMac } from './ble';
