import React from 'react';
import { DeviceInfo } from '../types';
import Spinner from './ui/Spinner';
import { formatDateTime } from '../services/formatters';

interface DeviceInfoPanelProps {
    deviceInfo: DeviceInfo | null;
    loading: boolean;
    error: string | null;
}

const InfoRow: React.FC<{ label: string; value: string | number | undefined }> = ({ label, value }) => (
    <div className="flex justify-between items-center py-2 border-b border-gray-700/50">
        <span className="text-gray-400">{label}</span>
        <span className="font-mono text-teal-300 text-left">{value ?? 'N/A'}</span>
    </div>
);


const DeviceInfoPanel: React.FC<DeviceInfoPanelProps> = ({ deviceInfo, loading, error }) => {
    
    const renderContent = () => {
        if (loading) return <div className="flex justify-center p-4 mt-4"><Spinner /></div>;
        if (error) return <p className="text-red-400 bg-red-900/50 p-3 rounded-md mt-4">{error}</p>;
        if (!deviceInfo) return <p className="text-gray-500 mt-4">اطلاعاتی برای دستگاه یافت نشد.</p>;

        return (
            <div className="space-y-1 text-sm mt-4">
               <InfoRow label="آخرین بازدید" value={formatDateTime(deviceInfo.timestamp)} />
               <InfoRow label="آدرس IP" value={deviceInfo.ip} />
               <InfoRow label="آدرس MAC" value={deviceInfo.mac} />
               <InfoRow label="قدرت سیگنال" value={`${deviceInfo.rssi} dBm`} />
            </div>
        );
    }

    return (
        <div className="mt-8">
             <h2 className="text-xl font-semibold mb-2 text-gray-300 border-b-2 border-gray-700 pb-2">وضعیت دستگاه</h2>
             {renderContent()}
        </div>
    );
};

export default DeviceInfoPanel;