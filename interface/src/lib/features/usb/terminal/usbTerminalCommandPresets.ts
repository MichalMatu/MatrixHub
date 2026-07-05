export interface UsbTerminalPresetOsOption {
	value: string;
	label: string;
}

export interface UsbTerminalCommandPreset {
	id: string;
	label: string;
	description: string;
	command: string;
}

export const USB_TERMINAL_PRESET_OS_OPTIONS: UsbTerminalPresetOsOption[] = [
	{ value: 'generic', label: 'Auto / Generic' },
	{ value: 'linux', label: 'Linux / Raspberry Pi' },
	{ value: 'macos', label: 'macOS' },
	{ value: 'powershell', label: 'Windows PowerShell' },
	{ value: 'cmd', label: 'Windows CMD' },
	{ value: 'android', label: 'Android / Termux' }
];

const PRESETS_BY_OS: Record<string, UsbTerminalCommandPreset[]> = {
	generic: [
		{
			id: 'generic-smoke',
			label: 'MatrixHub terminal smoke test',
			description: 'Prints a known marker, basic system info, and the current directory.',
			command: "printf 'MATRIXHUB_AUTO_OK\\n'; uname -a; pwd"
		},
		{
			id: 'generic-serial-ports',
			label: 'Detect serial ports',
			description: 'Lists common Linux, macOS, and Android serial device paths if they exist.',
			command:
				"printf 'Serial ports:\\n'; ls -l /dev/serial/by-id/* /dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/tty.usbmodem* /dev/cu.usbserial* /dev/tty.usbserial* 2>/dev/null"
		},
		{
			id: 'generic-user',
			label: 'Check current user',
			description: 'Shows the current user and group context.',
			command: 'id || whoami'
		},
		{
			id: 'generic-pwd',
			label: 'Print working directory',
			description: 'Shows the shell directory where MatrixHub is typing commands.',
			command: 'pwd'
		}
	],
	linux: [
		{
			id: 'linux-serial-ports',
			label: 'Detect serial ports',
			description: 'Lists stable by-id links and ttyACM/ttyUSB devices used by ESP32 boards.',
			command:
				"printf 'Serial ports:\\n'; ls -l /dev/serial/by-id/* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null"
		},
		{
			id: 'linux-user',
			label: 'Check current user',
			description: 'Shows UID, GID, and supplemental groups for USB permission checks.',
			command: 'id'
		},
		{
			id: 'linux-pwd',
			label: 'Print working directory',
			description: 'Shows the current shell directory.',
			command: 'pwd'
		},
		{
			id: 'linux-list',
			label: 'List files',
			description: 'Lists files in the current directory, including hidden entries.',
			command: 'ls -la'
		},
		{
			id: 'linux-usb-permissions',
			label: 'USB permissions check',
			description: 'Shows user groups and permissions on common serial device nodes.',
			command: 'id; groups; ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null'
		},
		{
			id: 'linux-smoke',
			label: 'MatrixHub terminal smoke test',
			description: 'Prints a known marker, kernel info, and the current directory.',
			command: "printf 'MATRIXHUB_LINUX_OK\\n'; uname -a; pwd"
		},
		{
			id: 'linux-network',
			label: 'Network info',
			description: 'Shows IP interfaces using iproute2, falling back to ifconfig.',
			command: 'ip addr show || ifconfig'
		},
		{
			id: 'linux-system',
			label: 'System info',
			description: 'Shows kernel details and uptime.',
			command: 'uname -a; uptime'
		}
	],
	macos: [
		{
			id: 'macos-serial-ports',
			label: 'Detect serial ports',
			description: 'Lists macOS USB modem and USB serial device nodes.',
			command:
				"printf 'Serial ports:\\n'; ls -l /dev/cu.usbmodem* /dev/tty.usbmodem* /dev/cu.usbserial* /dev/tty.usbserial* 2>/dev/null"
		},
		{
			id: 'macos-user',
			label: 'Check current user',
			description: 'Shows UID, GID, and groups for the current macOS shell.',
			command: 'id'
		},
		{
			id: 'macos-pwd',
			label: 'Print working directory',
			description: 'Shows the current shell directory.',
			command: 'pwd'
		},
		{
			id: 'macos-list',
			label: 'List files',
			description: 'Lists files in the current directory, including hidden entries.',
			command: 'ls -la'
		},
		{
			id: 'macos-usb-permissions',
			label: 'USB permissions check',
			description: 'Shows user groups and permissions on common macOS serial device nodes.',
			command:
				'id; groups; ls -l /dev/cu.usbmodem* /dev/tty.usbmodem* /dev/cu.usbserial* /dev/tty.usbserial* 2>/dev/null'
		},
		{
			id: 'macos-smoke',
			label: 'MatrixHub terminal smoke test',
			description: 'Prints a known marker, macOS version details, and the current directory.',
			command: "printf 'MATRIXHUB_MACOS_OK\\n'; sw_vers; pwd"
		},
		{
			id: 'macos-network',
			label: 'Network info',
			description: 'Shows macOS network interface details.',
			command: 'ifconfig'
		},
		{
			id: 'macos-system',
			label: 'System info',
			description: 'Shows kernel and macOS version details.',
			command: 'uname -a; sw_vers'
		}
	],
	powershell: [
		{
			id: 'powershell-serial-ports',
			label: 'Detect serial ports',
			description: 'Lists COM ports visible to .NET serial APIs.',
			command: '[System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object'
		},
		{
			id: 'powershell-user',
			label: 'Check current user',
			description: 'Shows the current Windows user.',
			command: 'whoami'
		},
		{
			id: 'powershell-pwd',
			label: 'Print working directory',
			description: 'Shows the current PowerShell location.',
			command: 'Get-Location'
		},
		{
			id: 'powershell-list',
			label: 'List files',
			description: 'Lists files in the current directory, including hidden entries.',
			command: 'Get-ChildItem -Force'
		},
		{
			id: 'powershell-usb-permissions',
			label: 'USB device check',
			description: 'Shows present USB, serial, and COM devices known to Windows.',
			command:
				"Get-PnpDevice -PresentOnly | Where-Object { $_.FriendlyName -match 'USB|Serial|COM' } | Select-Object Status,Class,FriendlyName,InstanceId"
		},
		{
			id: 'powershell-smoke',
			label: 'MatrixHub terminal smoke test',
			description: 'Prints a known marker, PowerShell version, and current location.',
			command: "Write-Output 'MATRIXHUB_POWERSHELL_OK'; $PSVersionTable.PSVersion; Get-Location"
		},
		{
			id: 'powershell-network',
			label: 'Network info',
			description: 'Shows Windows IP configuration through PowerShell.',
			command: 'Get-NetIPConfiguration'
		},
		{
			id: 'powershell-system',
			label: 'System info',
			description: 'Shows selected Windows OS and host details.',
			command: 'Get-ComputerInfo | Select-Object OsName,OsVersion,CsName'
		}
	],
	cmd: [
		{
			id: 'cmd-serial-ports',
			label: 'Detect serial ports',
			description: 'Lists Windows console device status, including COM ports when available.',
			command: 'mode'
		},
		{
			id: 'cmd-user',
			label: 'Check current user',
			description: 'Shows the current Windows user.',
			command: 'whoami'
		},
		{
			id: 'cmd-pwd',
			label: 'Print working directory',
			description: 'Shows the current CMD directory.',
			command: 'cd'
		},
		{
			id: 'cmd-list',
			label: 'List files',
			description: 'Lists files in the current directory, including hidden entries.',
			command: 'dir /a'
		},
		{
			id: 'cmd-usb-permissions',
			label: 'USB device check',
			description: 'Shows serial ports reported by Windows Management Instrumentation.',
			command: 'wmic path Win32_SerialPort get DeviceID,Name 2>NUL'
		},
		{
			id: 'cmd-smoke',
			label: 'MatrixHub terminal smoke test',
			description: 'Prints a known marker, Windows version, and current directory.',
			command: 'echo MATRIXHUB_CMD_OK & ver & cd'
		},
		{
			id: 'cmd-network',
			label: 'Network info',
			description: 'Shows Windows IP configuration.',
			command: 'ipconfig /all'
		},
		{
			id: 'cmd-system',
			label: 'System info',
			description: 'Shows Windows system information.',
			command: 'systeminfo'
		}
	],
	android: [
		{
			id: 'android-serial-ports',
			label: 'Detect serial ports',
			description: 'Lists common Android and Termux serial-like device nodes when accessible.',
			command:
				"printf 'Serial-like devices:\\n'; ls -l /dev/ttyACM* /dev/ttyUSB* /dev/ttyGS* 2>/dev/null"
		},
		{
			id: 'android-user',
			label: 'Check current user',
			description: 'Shows Android shell UID, GID, and groups.',
			command: 'id'
		},
		{
			id: 'android-pwd',
			label: 'Print working directory',
			description: 'Shows the current shell directory.',
			command: 'pwd'
		},
		{
			id: 'android-list',
			label: 'List files',
			description: 'Lists files in the current directory, including hidden entries.',
			command: 'ls -la'
		},
		{
			id: 'android-usb-permissions',
			label: 'USB permissions check',
			description: 'Shows shell identity, Android version, and accessible serial device nodes.',
			command: 'id; getprop ro.build.version.release; ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null'
		},
		{
			id: 'android-smoke',
			label: 'MatrixHub terminal smoke test',
			description: 'Prints a known marker, kernel info, and the current directory.',
			command: "printf 'MATRIXHUB_ANDROID_OK\\n'; uname -a; pwd"
		},
		{
			id: 'android-network',
			label: 'Network info',
			description: 'Shows Android network interfaces.',
			command: 'ip addr show || ifconfig'
		},
		{
			id: 'android-system',
			label: 'System info',
			description: 'Shows kernel info and Android device model.',
			command: 'uname -a; getprop ro.product.model'
		}
	]
};

export function getUsbTerminalPresetsForOs(os: string): UsbTerminalCommandPreset[] {
	return PRESETS_BY_OS[os] ?? PRESETS_BY_OS.generic;
}

export function findUsbTerminalPreset(
	os: string,
	presetId: string
): UsbTerminalCommandPreset | null {
	return getUsbTerminalPresetsForOs(os).find((preset) => preset.id === presetId) ?? null;
}
