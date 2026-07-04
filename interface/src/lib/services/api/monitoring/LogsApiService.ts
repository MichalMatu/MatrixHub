import { createApiClient, type ApiClientOptions, ApiError } from '$lib/utils';

interface ErrorPayload {
	error?: string;
	message?: string;
}

async function toApiError(response: Response, fallbackMessage: string): Promise<ApiError> {
	let errorCode: string | undefined;
	let serverMessage: string | undefined;

	try {
		const contentType = response.headers.get('content-type');
		if (contentType?.includes('application/json')) {
			const body = (await response.json()) as ErrorPayload;
			if (typeof body.error === 'string') {
				errorCode = body.error;
			}
			if (typeof body.message === 'string') {
				serverMessage = body.message;
			}
		}
	} catch {
		// Fall back to the generic message when the error body is absent or malformed.
	}

	return new ApiError(response.status, fallbackMessage, serverMessage ?? errorCode, errorCode);
}

export interface LogFile {
	name: string;
	size: number;
}

export interface LogMonth {
	name: string;
	path: string;
	files: LogFile[];
}

export interface LogListResponse {
	total_size?: number;
	months: LogMonth[];
}

export class LogsApiService {
	private client;
	private static readonly LIST_TIMEOUT_MS = 15000;
	private static readonly DOWNLOAD_TIMEOUT_MS = 30000; // Increased for large files
	private static readonly DELETE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getLogsList(): Promise<LogListResponse> {
		return this.client.get<LogListResponse>('/api/logs', {
			signal: AbortSignal.timeout(LogsApiService.LIST_TIMEOUT_MS)
		});
	}

	async getHistoricalChartData(date: string): Promise<ArrayBuffer> {
		const [year, month] = date.split('-');
		const binFile = `/data/${year}-${month}/${date}.bin`;
		const url = `/api/logs/download?file=${encodeURIComponent(binFile)}`;

		// Use client.fetch which handles auth headers automatically
		const res = await this.client.fetch(url, {
			signal: AbortSignal.timeout(LogsApiService.DOWNLOAD_TIMEOUT_MS)
		});
		if (res.status === 404) {
			throw new ApiError(res.status, `GET ${url} failed`, 'File not found');
		}
		if (!res.ok) {
			throw await toApiError(res, `GET ${url} failed`);
		}
		return res.arrayBuffer();
	}

	async downloadLog(fullPath: string): Promise<Blob> {
		const url = `/api/logs/download?file=${encodeURIComponent(fullPath)}`;
		const res = await this.client.fetch(url, {
			signal: AbortSignal.timeout(LogsApiService.DOWNLOAD_TIMEOUT_MS)
		});
		if (res.status === 404) {
			throw new ApiError(res.status, `GET ${url} failed`, 'File not found');
		}
		if (!res.ok) {
			throw await toApiError(res, `GET ${url} failed`);
		}
		return res.blob();
	}

	async deleteLog(fullPath: string): Promise<void> {
		const url = `/api/logs/delete?file=${encodeURIComponent(fullPath)}`;
		await this.client.delete<{ ok: boolean; status: string }>(url, {
			method: 'DELETE',
			signal: AbortSignal.timeout(LogsApiService.DELETE_TIMEOUT_MS)
		});
	}
}
