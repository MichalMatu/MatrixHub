/**
 * Centralized Logger service for the application.
 * Currently wraps standard console methods, but provides a single point
 * for future extensions (e.g., sending logs to a remote server,
 * filtering by environment, or formatting).
 */
export class Logger {
	private static isTestEnv() {
		return import.meta.env.MODE === 'test';
	}

	static debug(message: string, ...args: unknown[]) {
		if (import.meta.env.DEV && !Logger.isTestEnv()) {
			console.debug(`[DEBUG] ${message}`, ...args);
		}
	}

	static info(message: string, ...args: unknown[]) {
		if (Logger.isTestEnv()) return;
		console.info(`[INFO] ${message}`, ...args);
	}

	static warn(message: string, ...args: unknown[]) {
		if (Logger.isTestEnv()) return;
		console.warn(`[WARN] ${message}`, ...args);
	}

	static error(message: string, ...args: unknown[]) {
		if (Logger.isTestEnv()) return;
		console.error(`[ERROR] ${message}`, ...args);
	}
}
