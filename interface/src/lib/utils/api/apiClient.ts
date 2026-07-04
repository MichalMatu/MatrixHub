/**
 * Centralized API client utilities
 * Provides consistent auth headers and fetch wrappers across all components
 */
import type { ZodSchema } from 'zod';

export interface ApiRequestInit<T> extends RequestInit {
	schema?: ZodSchema<T>;
}

export interface ApiClientOptions {
	bearerToken: string;
	fetch?: typeof fetch;
	onUnauthorized?: () => void;
}

/**
 * Get common headers for API requests
 */
export function getApiHeaders(options: ApiClientOptions): Record<string, string> {
	const headers: Record<string, string> = {
		'Content-Type': 'application/json',
		Accept: 'application/json'
	};

	// Always send Authorization if we have a token.
	// Public endpoints use a client without a bearer token.
	if (options.bearerToken) {
		headers.Authorization = `Bearer ${options.bearerToken}`;
	}

	return headers;
}

/**
 * Create API client with pre-configured auth
 * @param options - API client options with optional bearer token and 401 handler
 */

/**
 * Helper to parse error response body and extract server error details
 */
interface ParsedError {
	errorCode?: string;
	message?: string;
}

async function parseErrorResponse(response: Response): Promise<ParsedError> {
	try {
		const contentType = response.headers.get('content-type');
		if (contentType && contentType.includes('application/json')) {
			const body = await response.json();
			if (body && typeof body.error === 'string') {
				return {
					errorCode: body.error,
					message: typeof body.message === 'string' ? body.message : undefined
				};
			}
		}
	} catch {
		// Failed to parse - return empty
	}
	return {};
}

// ── Global 401 handler ───────────────────────────────────────────────
// Set once at app init (e.g. in +layout.svelte) so that ANY apiClient
// triggers logout on 401, even services created directly.
let _globalOnUnauthorized: (() => void) | null = null;

/** Register a global callback that fires on any 401 response. */
export function setGlobalUnauthorizedHandler(handler: () => void) {
	_globalOnUnauthorized = handler;
}

export type ApiClient = ReturnType<typeof createApiClient>;

class ConcurrencyLimiter {
	private maxConcurrent: number;
	private currentConcurrent: number = 0;
	private queue: (() => void)[] = [];

	constructor(maxConcurrent: number) {
		this.maxConcurrent = maxConcurrent;
	}

	async acquire(): Promise<void> {
		if (this.currentConcurrent < this.maxConcurrent) {
			this.currentConcurrent++;
			return Promise.resolve();
		}
		return new Promise((resolve) => {
			this.queue.push(resolve);
		});
	}

	release(): void {
		if (this.queue.length > 0) {
			const next = this.queue.shift();
			if (next) next();
		} else {
			this.currentConcurrent--;
		}
	}
}

// Global Limiter - ESP32 has max 13 sockets, so 5 is a safe concurrent limit.
const globalApiLimiter = new ConcurrencyLimiter(5);

export function createApiClient(options: ApiClientOptions) {
	// Store options for dynamic header generation
	// Note: For reactive stores, the caller should create a new client when values change
	const getHeaders = () => getApiHeaders(options);
	const rawFetch = options.fetch ?? fetch;

	const fetchImpl = async (input: RequestInfo | URL, init?: RequestInit) => {
		const controller = new AbortController();
		const timeoutId = setTimeout(() => controller.abort(), 15000); // 15s timeout

		await globalApiLimiter.acquire();
		try {
			const response = await rawFetch(input, {
				...init,
				signal: init?.signal ?? controller.signal
			});
			// 401 is the single session-invalid signal after removing client-side
			// expiry checks. Everything funnels through this path.
			if (response.status === 401) {
				const handler = options.onUnauthorized ?? _globalOnUnauthorized;
				handler?.();
			}
			return response;
		} finally {
			globalApiLimiter.release();
			clearTimeout(timeoutId);
		}
	};

	const mergeHeaders = (init?: RequestInit): HeadersInit => {
		return {
			...getHeaders(), // Get fresh headers each time
			...(init?.headers ?? {})
		};
	};

	return {
		/**
		 * GET request
		 */
		async get<T>(url: string, init?: ApiRequestInit<T>): Promise<T> {
			const response = await fetchImpl(url, {
				method: 'GET',
				headers: mergeHeaders(init),
				...init
			});
			if (!response.ok) {
				const { errorCode, message: serverMsg } = await parseErrorResponse(response);
				throw new ApiError(response.status, `GET ${url} failed`, serverMsg ?? errorCode, errorCode);
			}
			const data = await response.json();
			if (init?.schema) {
				return init.schema.parse(data);
			}
			return data;
		},

		/**
		 * POST request
		 */
		/**
		 * POST request expecting JSON response
		 */
		async post<T>(url: string, body?: unknown, init?: ApiRequestInit<T>): Promise<T> {
			const response = await fetchImpl(url, {
				method: 'POST',
				headers: mergeHeaders(init),
				body: body ? JSON.stringify(body) : undefined,
				...init
			});
			if (!response.ok) {
				const { errorCode, message: serverMsg } = await parseErrorResponse(response);
				throw new ApiError(
					response.status,
					`POST ${url} failed`,
					serverMsg ?? errorCode,
					errorCode
				);
			}
			// Check if response has JSON content
			const contentType = response.headers.get('content-type');
			if (contentType && contentType.includes('application/json')) {
				const data = await response.json();
				if (init?.schema) {
					return init.schema.parse(data);
				}
				return data;
			}
			throw new ApiError(
				response.status,
				`POST ${url} returned non-JSON response`,
				'Expected JSON response'
			);
		},

		/**
		 * POST request expecting no content (e.g. 204 or ignored return)
		 */
		async postVoid(url: string, body?: unknown, init?: RequestInit): Promise<void> {
			const response = await fetchImpl(url, {
				method: 'POST',
				headers: mergeHeaders(init),
				body: body ? JSON.stringify(body) : undefined,
				...init
			});
			if (!response.ok) {
				const { errorCode, message: serverMsg } = await parseErrorResponse(response);
				throw new ApiError(
					response.status,
					`POST ${url} failed`,
					serverMsg ?? errorCode,
					errorCode
				);
			}
		},

		/**
		 * PUT request
		 */
		async put<T>(url: string, body: unknown, init?: ApiRequestInit<T>): Promise<T> {
			const response = await fetchImpl(url, {
				method: 'PUT',
				headers: mergeHeaders(init),
				body: JSON.stringify(body),
				...init
			});
			if (!response.ok) {
				const { errorCode, message: serverMsg } = await parseErrorResponse(response);
				throw new ApiError(response.status, `PUT ${url} failed`, serverMsg ?? errorCode, errorCode);
			}
			const data = await response.json();
			if (init?.schema) {
				return init.schema.parse(data);
			}
			return data;
		},

		/**
		 * DELETE request
		 */
		async delete<T>(url: string, init?: ApiRequestInit<T>): Promise<T> {
			const response = await fetchImpl(url, {
				method: 'DELETE',
				headers: mergeHeaders(init),
				...init
			});
			if (!response.ok) {
				const { errorCode, message: serverMsg } = await parseErrorResponse(response);
				throw new ApiError(
					response.status,
					`DELETE ${url} failed`,
					serverMsg ?? errorCode,
					errorCode
				);
			}
			const data = await response.json();
			if (init?.schema) {
				return init.schema.parse(data);
			}
			return data;
		},

		/**
		 * Raw fetch with configured headers (for custom handling)
		 */
		async fetch(url: string, init?: RequestInit): Promise<Response> {
			// When body is FormData, let browser set Content-Type with correct
			// multipart boundary. Forcing 'application/json' breaks multipart uploads.
			const defaults = getHeaders();
			if (init?.body instanceof FormData) {
				delete defaults['Content-Type'];
			}
			return fetchImpl(url, {
				...init,
				headers: {
					...defaults,
					...init?.headers
				}
			});
		}
	};
}

/**
 * Custom API error class with status code and optional server message
 */
export class ApiError extends Error {
	public serverMessage?: string;
	public errorCode?: string;

	constructor(
		public status: number,
		message: string,
		serverMessage?: string,
		errorCode?: string
	) {
		// Use server message as the main message if available
		super(serverMessage || message);
		this.name = 'ApiError';
		this.serverMessage = serverMessage;
		this.errorCode = errorCode;
	}

	get isUnauthorized(): boolean {
		return this.status === 401;
	}

	get isForbidden(): boolean {
		return this.status === 403;
	}

	get isNotFound(): boolean {
		return this.status === 404;
	}

	get isServerError(): boolean {
		return this.status >= 500;
	}

	get isBadRequest(): boolean {
		return this.status === 400;
	}
}
