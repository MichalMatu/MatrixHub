import * as m from '$lib/paraglide/messages.js';

export function localizeTerminalMessage(
	message: string | null | undefined,
	locale: string
): string | null {
	if (!message) return null;

	const localeOptions = { locale };
	switch (message) {
		case 'USB terminal console requires security to be enabled.':
			return m.usb_terminal_console_requires_security(localeOptions);
		case 'Failed to parse USB terminal response.':
			return m.usb_terminal_parse_error(localeOptions);
		case 'Terminal request failed.':
			return m.usb_terminal_request_failed(localeOptions);
		case 'USB terminal connection error.':
			return m.usb_terminal_connection_error(localeOptions);
		case 'Terminal connection established.':
			return m.usb_terminal_connection_established(localeOptions);
		case 'USB terminal connection lost. Reconnecting...':
			return m.usb_terminal_connection_lost_reconnecting(localeOptions);
		case 'USB terminal is not connected.':
			return m.usb_terminal_not_connected(localeOptions);
		case 'USB terminal error.':
			return m.usb_terminal_error_generic(localeOptions);
		case 'USB Terminal Service is disabled. Enable it in the Web UI.':
			return m.usb_terminal_message_disabled_service(localeOptions);
		case 'Terminal is busy with another session.':
			return m.usb_terminal_message_busy_other_session(localeOptions);
		case 'Terminal is busy processing the previous command. Use status or cancel.':
			return m.usb_terminal_message_busy_own_session(localeOptions);
		case 'Internal error: failed to allocate terminal output buffer.':
			return m.usb_terminal_message_internal_buffer(localeOptions);
		case 'Internal error: failed to type the command.':
			return m.usb_terminal_message_internal_command(localeOptions);
		case 'Port not set. Typing blindly without feedback.':
			return m.usb_terminal_message_port_not_set(localeOptions);
		case 'Terminal is idle.':
			return m.usb_terminal_message_idle(localeOptions);
		case 'Command is still running.':
			return m.usb_terminal_message_running(localeOptions);
		case 'Command is running, but no output has been captured yet.':
			return m.usb_terminal_message_running_no_output(localeOptions);
		case 'Interrupt signal (Ctrl+C) sent to host.':
			return m.usb_terminal_message_interrupted(localeOptions);
		case 'USB terminal service is unavailable.':
			return m.usb_terminal_message_service_unavailable(localeOptions);
		case 'Invalid terminal request payload.':
			return m.usb_terminal_message_invalid_request(localeOptions);
		case 'Unsupported terminal request type.':
			return m.usb_terminal_message_unsupported_request(localeOptions);
		default:
			return message;
	}
}
