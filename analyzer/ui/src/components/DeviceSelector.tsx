import React, { useState, useEffect } from 'react';
import { fetchDevices } from '../services/api';
import Spinner from './ui/Spinner';

interface DeviceSelectorProps {
    onDeviceSelect: (device: string | null) => void;
    selectedDevice: string | null;
}

const DeviceSelector: React.FC<DeviceSelectorProps> = ({ onDeviceSelect, selectedDevice }) => {
    const [devices, setDevices] = useState<string[]>([]);
    const [loading, setLoading] = useState<boolean>(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        const getDevices = async () => {
            try {
                setLoading(true);
                setError(null);
                const data = await fetchDevices();
                setDevices(data.devices);
            } catch (err: any) {
                setError(err.message || 'خطا در دریافت لیست دستگاه‌ها.');
            } finally {
                setLoading(false);
            }
        };
        getDevices();
    }, []);

    const handleChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
        const device = e.target.value;
        onDeviceSelect(device || null);
    };

    return (
        <div className="mb-8">
            <h2 className="text-xl font-semibold mb-4 text-gray-300 border-b-2 border-gray-700 pb-2">انتخاب دستگاه</h2>
            {loading && <div className="flex justify-center p-4"><Spinner /></div>}
            {error && <p className="text-red-400 bg-red-900/50 p-3 rounded-md">{error}</p>}
            {!loading && !error && (
                <select 
                    value={selectedDevice || ''}
                    onChange={handleChange}
                    className="w-full p-3 bg-gray-700 border border-gray-600 rounded-md focus:outline-none focus:ring-2 focus:ring-teal-500 text-white"
                >
                    <option value="">-- یک دستگاه انتخاب کنید --</option>
                    {devices.map(device => (
                        <option key={device} value={device}>{device}</option>
                    ))}
                </select>
            )}
        </div>
    );
};

export default DeviceSelector;