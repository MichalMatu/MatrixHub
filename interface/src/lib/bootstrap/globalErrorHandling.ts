const CHUNK_RELOAD_WINDOW_MS = 30000;
const CHUNK_RELOAD_KEY = 'matrixhub:last-chunk-reload';
const OVERLAY_ID = 'sys-crash-overlay';
const INSTALL_GUARD = '__matrixhubGlobalErrorHandlingInstalled__';

function getErrorName(error: unknown): string | null {
	if (
		error &&
		typeof error === 'object' &&
		'name' in error &&
		typeof (error as { name: unknown }).name === 'string'
	) {
		return (error as { name: string }).name;
	}

	return null;
}

function toErrorMessage(error: unknown): string {
	if (error instanceof Error) return error.message;
	if (typeof error === 'string') return error;

	try {
		return JSON.stringify(error);
	} catch {
		return String(error);
	}
}

export function isAbortLikeError(error: unknown): boolean {
	const name = getErrorName(error);
	const message = toErrorMessage(error).toLowerCase();

	if (name === 'AbortError' || name === 'TimeoutError') {
		return true;
	}

	return message.includes('aborted');
}

export function extractChunkAssetId(message: unknown): string {
	const text = toErrorMessage(message);
	const absolute = text.match(/https?:\/\/\S+\/_app\/immutable\/\S+?\.js(?:\?\S*)?/);
	if (absolute) return absolute[0];

	const relative = text.match(/\/_app\/immutable\/\S+?\.js(?:\?\S*)?/);
	return relative ? relative[0] : 'dynamic-import';
}

export function isRecoverableChunkError(message: unknown): boolean {
	return /Failed to fetch dynamically imported module|Importing a module script failed|Loading chunk [\w-]+ failed/i.test(
		toErrorMessage(message)
	);
}

function shouldReloadAfterChunkError(storage: Storage, message: unknown, now: number): boolean {
	const asset = extractChunkAssetId(message);
	const previous = storage.getItem(CHUNK_RELOAD_KEY);
	if (previous) {
		try {
			const parsed = JSON.parse(previous) as { asset?: string; ts?: number } | null;
			if (
				parsed?.asset === asset &&
				typeof parsed.ts === 'number' &&
				now - parsed.ts < CHUNK_RELOAD_WINDOW_MS
			) {
				return false;
			}
		} catch {
			// Ignore broken persisted state and keep best-effort recovery.
		}
	}

	storage.setItem(CHUNK_RELOAD_KEY, JSON.stringify({ asset, ts: now }));
	return true;
}

export function renderFatalErrorOverlay(
	documentRef: Document,
	type: string,
	message: string,
	reload: () => void
) {
	if (documentRef.getElementById(OVERLAY_ID)) return;

	const overlay = documentRef.createElement('div');
	overlay.id = OVERLAY_ID;
	overlay.style.cssText =
		'position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#ef4444;color:white;z-index:99999;padding:12px 24px;border-radius:8px;font-family:system-ui;box-shadow:0 4px 6px rgba(0,0,0,0.1);max-width:90%;word-break:break-word;text-align:center;font-size:14px;';

	const title = documentRef.createElement('strong');
	title.textContent = type;

	const messageBreak = documentRef.createElement('br');

	const messageText = documentRef.createElement('small');
	messageText.style.opacity = '0.9';
	messageText.textContent = message || 'Unknown error occurred';

	const buttonBreak = documentRef.createElement('br');

	const button = documentRef.createElement('button');
	button.type = 'button';
	button.textContent = 'Reload Application';
	button.style.cssText =
		'margin-top:10px;background:rgba(255,255,255,0.25);border:none;padding:6px 16px;border-radius:6px;color:white;cursor:pointer;font-weight:bold;';
	button.addEventListener('click', reload);

	overlay.append(title, messageBreak, messageText, buttonBreak, button);

	const append = () => {
		if (!documentRef.getElementById(OVERLAY_ID)) {
			documentRef.body?.appendChild(overlay);
		}
	};

	if (documentRef.body) {
		append();
	} else {
		documentRef.addEventListener('DOMContentLoaded', append, { once: true });
	}
}

export function installGlobalErrorHandling() {
	if (typeof window === 'undefined' || typeof document === 'undefined') {
		return () => {};
	}

	const guardedWindow = window as Window & {
		[INSTALL_GUARD]?: boolean;
	};
	if (guardedWindow[INSTALL_GUARD]) {
		return () => {};
	}
	guardedWindow[INSTALL_GUARD] = true;

	const maybeRecoverChunkError = (message: unknown) => {
		if (!isRecoverableChunkError(message)) return false;

		try {
			if (!shouldReloadAfterChunkError(window.sessionStorage, message, Date.now())) {
				return false;
			}
		} catch {
			return false;
		}

		window.location.reload();
		return true;
	};

	const showFatalError = (type: string, message: string) => {
		renderFatalErrorOverlay(document, type, message, () => window.location.reload());
	};

	const handleError = (event: ErrorEvent) => {
		const sourceError = event.error ?? event.message;
		if (isAbortLikeError(sourceError)) return;
		if (maybeRecoverChunkError(sourceError)) return;

		showFatalError('Application Error', toErrorMessage(sourceError) || 'Unknown error occurred');
	};

	const handleUnhandledRejection = (event: PromiseRejectionEvent) => {
		if (isAbortLikeError(event.reason)) {
			event.preventDefault();
			return;
		}

		const message = toErrorMessage(event.reason);
		if (maybeRecoverChunkError(message)) {
			event.preventDefault();
			return;
		}

		showFatalError('Connection or Async Error', message || 'Unknown error occurred');
	};

	window.addEventListener('error', handleError);
	window.addEventListener('unhandledrejection', handleUnhandledRejection);

	return () => {
		window.removeEventListener('error', handleError);
		window.removeEventListener('unhandledrejection', handleUnhandledRejection);
		guardedWindow[INSTALL_GUARD] = false;
	};
}
