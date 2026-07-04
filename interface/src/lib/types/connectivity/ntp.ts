/**
 * NTP (Network Time Protocol) types
 */

export type NTPSettings = {
	enabled: boolean;
	server: string;
	tz_label: string;
	tz_format: string;
};

export type NTPStatus = {
	status: number;
	// During early boot or after a bad RTC/TZ transition the backend may return
	// empty formatted timestamps on purpose instead of failing the whole status
	// request. The UI uses this flag/empty-string combination to render a safe
	// placeholder until time becomes trustworthy again.
	time_valid?: boolean;
	utc_time: string;
	local_time: string;
	server: string;
	uptime: number;
};
