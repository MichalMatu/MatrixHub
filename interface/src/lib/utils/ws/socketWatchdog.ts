interface SocketWatchdogOptions {
	timeoutMs: number;
	onTimeout: () => void;
}

export class SocketWatchdog {
	private timer: ReturnType<typeof setTimeout> | null = null;

	constructor(private readonly options: SocketWatchdogOptions) {}

	arm(enabled = true) {
		this.clear();
		if (!enabled) return;

		this.timer = setTimeout(() => {
			this.timer = null;
			this.options.onTimeout();
		}, this.options.timeoutMs);
	}

	clear() {
		if (!this.timer) return;
		clearTimeout(this.timer);
		this.timer = null;
	}

	destroy() {
		this.clear();
	}
}
