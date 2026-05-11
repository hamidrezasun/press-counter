export interface DeviceList {
    devices: string[];
}

export interface DeviceInfo {
    timestamp: string;
    ip: string;
    rssi: number;
    mac: string;
}

export interface DeviceInfoList {
    records: DeviceInfo[];
}

export interface PressCounterRecord {
    timestamp: string;
    count: number;
    duration: number;
    temperature: number;
}

export interface PressCounterList {
    records: PressCounterRecord[];
}

export interface WorkSession {
    session_id: string;
    device: string;
    personnel_id: string;
    start_time: string;
    end_time: string | null;
    duration_seconds: number | null;
    missing_counts: number[];
    average_press_interval_seconds: number;
    average_temperature: number;
    average_press_duration_ms: number;
    total_count: number;
    die_id: string | null;
    end_reason: string | null;
}

export interface WorkSessionList {
    sessions: WorkSession[];
}

export interface DeviceStatus {
    relayStatus: 'On' | 'Off';
    buzzerStatus: 'On' | 'Off';
    buttonStatus: 'Pressed' | 'Not Pressed';
    tagData: string | null;
}

export interface PulseCounter {
    count: number;
    lastPulseDuration: number;
    lastPulseTime: number; // Unix timestamp
    inPulseTrain: boolean;
}

export interface DeviceSettings {
    device?: string;
    influxHost?: string;
    influxPort?: number;
    influxBucket?: string;
    influxOrg?: string;
    influxToken?: string;
    minPulseDuration?: number;
}