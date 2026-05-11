import { DeviceList, WorkSessionList, PressCounterList, DeviceInfoList, DeviceStatus, PulseCounter, DeviceSettings } from './types';

const BASE_URL = 'http://localhost:8000';

async function handleResponse<T>(response: Response): Promise<T> {
    if (!response.ok) {
        const errorText = await response.text();
        try {
            const errorData = JSON.parse(errorText);
            throw new Error(errorData.detail || `HTTP error! status: ${response.status}`);
        } catch (e) {
             throw new Error(errorText || `HTTP error! status: ${response.status}`);
        }
    }
    const contentType = response.headers.get('content-type');
    if (contentType && contentType.includes('application/json')) {
        return response.json() as Promise<T>;
    }
    // For text responses from device proxy
    return response.text() as unknown as Promise<T>;
}

export async function fetchDevices(): Promise<DeviceList> {
    const response = await fetch(`${BASE_URL}/devices/`);
    return handleResponse<DeviceList>(response);
}

export async function fetchDeviceInfo(device: string): Promise<DeviceInfoList> {
    const response = await fetch(`${BASE_URL}/devices/${encodeURIComponent(device)}/info`);
    return handleResponse<DeviceInfoList>(response);
}

export async function fetchWorkSessions(
    device: string | null,
    filter: { days?: number; } = {}
): Promise<WorkSessionList> {
    const url = new URL(device ? `${BASE_URL}/sessions/${encodeURIComponent(device)}` : `${BASE_URL}/sessions/`);
    if (filter.days !== undefined) {
        url.searchParams.append('days', filter.days.toString());
    }
    const response = await fetch(url.toString());
    return handleResponse<WorkSessionList>(response);
}

export async function fetchSessionPresses(device: string, begin_date: string, end_date: string | null): Promise<PressCounterList> {
    // If the session is ongoing (end_date is null), use the current time as the end of the range.
    const finalEndDate = end_date ? end_date : new Date().toISOString();
    const url = `${BASE_URL}/sessions/${encodeURIComponent(device)}/${encodeURIComponent(begin_date)}/${encodeURIComponent(finalEndDate)}`;
    
    const response = await fetch(url);
    return handleResponse<PressCounterList>(response);
}


// --- Device Control Proxy Functions ---

export async function fetchDeviceStatus(ip: string): Promise<DeviceStatus> {
    const response = await fetch(`http://${ip}/status`);
    return handleResponse<DeviceStatus>(response);
}

export async function fetchDevicePulseCounter(ip: string): Promise<PulseCounter> {
    const response = await fetch(`http://${ip}/press_counter`);
    return handleResponse<PulseCounter>(response);
}

export async function postDeviceControl(ip: string, command: string): Promise<any> {
    const response = await fetch(`http://${ip}/${command}`, { method: 'POST' });
    return handleResponse<any>(response);
}

export async function fetchDeviceSettings(ip: string): Promise<DeviceSettings> {
    const response = await fetch(`http://${ip}/save`);
    return handleResponse<DeviceSettings>(response);
}

export async function saveDeviceSettings(ip: string, settingsData: FormData): Promise<any> {
    const response = await fetch(`http://${ip}/save`, {
        method: 'POST',
        body: settingsData,
    });
    return handleResponse<any>(response);
}