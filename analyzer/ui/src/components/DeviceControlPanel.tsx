import React, { useState, useEffect, useCallback, ChangeEvent } from 'react';
import { DeviceInfo, DeviceStatus, PulseCounter, DeviceSettings } from '../types';
import Card from './ui/Card';
import { fetchDeviceStatus, fetchDevicePulseCounter, postDeviceControl, fetchDeviceSettings, saveDeviceSettings } from '../services/api';


interface DeviceControlPanelProps {
    deviceInfo: DeviceInfo | null;
}

type Tab = 'controls' | 'settings' | 'firmware';

const StatusBarItem: React.FC<{ label: string; value: React.ReactNode; }> = ({ label, value }) => (
    <div className="text-center">
        <span className="text-sm font-semibold text-gray-400 block">{label}</span>
        <span className="mt-1 inline-block bg-gray-900/50 text-teal-300 font-mono px-3 py-1 rounded-md text-sm">{value}</span>
    </div>
);

const TabButton: React.FC<{label: string; active: boolean; onClick: () => void;}> = ({label, active, onClick}) => (
    <button 
        onClick={onClick}
        className={`px-4 py-2 text-sm font-medium rounded-t-lg transition-colors focus:outline-none ${active ? 'bg-gray-800 border-b-2 border-teal-500 text-white' : 'text-gray-400 hover:bg-gray-700/50'}`}
    >
        {label}
    </button>
)

const DeviceControlPanel: React.FC<DeviceControlPanelProps> = ({ deviceInfo }) => {
    const [activeTab, setActiveTab] = useState<Tab>('controls');
    const [status, setStatus] = useState<DeviceStatus | null>(null);
    const [pulseCounter, setPulseCounter] = useState<PulseCounter | null>(null);
    const [settings, setSettings] = useState<DeviceSettings>({});
    const [notification, setNotification] = useState<{ message: string; type: 'success' | 'error' } | null>(null);

    const ip = deviceInfo?.ip;

    const showNotification = (message: string, type: 'success' | 'error') => {
        setNotification({ message, type });
        setTimeout(() => setNotification(null), 5000);
    };

    const fetchStatus = useCallback(async () => {
        if (!ip) return;
        try {
            const data = await fetchDeviceStatus(ip);
            if (data) setStatus(data);
        } catch (err: any) {
             console.error(`API request to /status failed:`, err.message);
             // Do not show notification for regular polling failures
        }
    }, [ip]);

    const fetchPulseCounter = useCallback(async () => {
        if (!ip) return;
        try {
            const data = await fetchDevicePulseCounter(ip);
            if (data) setPulseCounter(data);
        } catch(err: any) {
            console.error(`API request to /press_counter failed:`, err.message);
            showNotification(err.message || 'خطا در دریافت شمارنده پالس.', 'error');
        }
    }, [ip]);


    useEffect(() => {
        fetchStatus();
        fetchPulseCounter();
        const interval = setInterval(fetchStatus, 5000);
        return () => clearInterval(interval);
    }, [fetchStatus, fetchPulseCounter]);

    const handleControlClick = async (command: string, successMessage: string) => {
        if (!ip) return;
        try {
            await postDeviceControl(ip, command);
            showNotification(successMessage, 'success');
            // Refresh status immediately after action
            setTimeout(fetchStatus, 500);
        } catch (err: any) {
            showNotification(err.message || 'عملیات ناموفق بود.', 'error');
        }
    };

    const handleLoadSettings = async () => {
        if (!ip) return;
        try {
            const data = await fetchDeviceSettings(ip);
            if (data) {
                setSettings(data);
                showNotification('تنظیمات با موفقیت بارگذاری شد.', 'success');
            }
        } catch (err: any) {
             showNotification(err.message || 'خطا در بارگذاری تنظیمات.', 'error');
        }
    };

    const handleSaveSettings = async () => {
        if (!ip) return;
        const formData = new FormData();
        Object.entries(settings).forEach(([key, value]) => {
            if (value !== undefined && value !== null) {
                formData.append(key, String(value));
            }
        });
        
        try {
            await saveDeviceSettings(ip, formData);
            showNotification('تنظیمات با موفقیت ذخیره شد.', 'success');
        } catch (err: any) {
            showNotification(err.message || 'خطا در ذخیره تنظیمات.', 'error');
        }
    };

    const handleSettingsChange = (e: ChangeEvent<HTMLInputElement>) => {
        const { name, value, type } = e.target;
        setSettings(prev => ({...prev, [name]: type === 'number' ? Number(value) : value}));
    };

    if (!ip) {
        return <Card><p className="text-center text-yellow-400">آدرس IP دستگاه در دسترس نیست. امکان برقراری ارتباط مستقیم وجود ندارد.</p></Card>;
    }

    const renderStatusValue = (value: string, onValue: string, onText: string, offText: string) => (
        <span className={`font-bold ${value === onValue ? 'text-green-400' : 'text-red-400'}`}>{value === onValue ? onText : offText}</span>
    );

    return (
        <div className="space-y-6">
             {notification && (
                <div className={`p-4 rounded-lg text-white ${notification.type === 'success' ? 'bg-green-500/80' : 'bg-red-500/80'}`}>
                    {notification.message}
                </div>
            )}
            <Card>
                <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                    <StatusBarItem label="رله" value={status ? renderStatusValue(status.relayStatus, 'On', 'روشن', 'خاموش') : '...'} />
                    <StatusBarItem label="بازر" value={status ? renderStatusValue(status.buzzerStatus, 'On', 'روشن', 'خاموش') : '...'} />
                    <StatusBarItem label="دکمه" value={status ? renderStatusValue(status.buttonStatus, 'Pressed', 'فشرده', 'آزاد') : '...'} />
                    <StatusBarItem label="تگ NFC" value={status?.tagData || 'N/A'} />
                </div>
            </Card>

            <div className="border-b border-gray-700 flex space-x-2">
                <TabButton label="کنترل‌ها" active={activeTab === 'controls'} onClick={() => setActiveTab('controls')} />
                <TabButton label="تنظیمات" active={activeTab === 'settings'} onClick={() => setActiveTab('settings')} />
                <TabButton label="سیستم‌عامل" active={activeTab === 'firmware'} onClick={() => setActiveTab('firmware')} />
            </div>

            {activeTab === 'controls' && (
                <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                    <Card>
                        <h3 className="text-lg font-semibold mb-4">رله و بازر</h3>
                        <div className="space-y-2">
                            <button onClick={() => handleControlClick('relay', 'وضعیت رله تغییر کرد.')} className="w-full bg-blue-600 hover:bg-blue-500 text-white font-bold py-2 px-4 rounded-lg transition-colors">تغییر وضعیت رله</button>
                            <button onClick={() => handleControlClick('buzzer_success', 'صدای موفقیت پخش شد.')} className="w-full bg-green-600 hover:bg-green-500 text-white font-bold py-2 px-4 rounded-lg transition-colors">پخش صدای موفقیت</button>
                            <button onClick={() => handleControlClick('buzzer_warning', 'صدای هشدار پخش شد.')} className="w-full bg-yellow-500 hover:bg-yellow-400 text-white font-bold py-2 px-4 rounded-lg transition-colors">پخش صدای هشدار</button>
                            <button onClick={() => handleControlClick('buzzer_error', 'صدای خطا پخش شد.')} className="w-full bg-red-600 hover:bg-red-500 text-white font-bold py-2 px-4 rounded-lg transition-colors">پخش صدای خطا</button>
                        </div>
                    </Card>
                    <Card>
                        <div className="flex justify-between items-center mb-4">
                           <h3 className="text-lg font-semibold">شمارنده پالس</h3>
                           <button onClick={fetchPulseCounter} className="text-sm bg-gray-700 hover:bg-gray-600 py-1 px-3 rounded-lg">تازه‌سازی</button>
                        </div>
                        <div className="space-y-3">
                            <div className="flex justify-between"><span className="text-gray-400">تعداد کل:</span> <span className="font-mono text-2xl text-teal-300">{pulseCounter?.count ?? '...'}</span></div>
                            <div className="flex justify-between"><span className="text-gray-400">آخرین زمان پالس:</span> <span className="font-mono">{pulseCounter?.lastPulseDuration ?? '...'} میلی‌ثانیه</span></div>
                            <div className="flex justify-between"><span className="text-gray-400">آخرین پالس:</span> <span className="font-mono">{pulseCounter?.lastPulseTime ? new Date(pulseCounter.lastPulseTime * 1000).toLocaleString('fa-IR') : 'هرگز'}</span></div>
                            <div className="flex justify-between"><span className="text-gray-400">وضعیت قطار پالس:</span> <span className="font-mono">{pulseCounter?.inPulseTrain ? 'فعال' : 'غیرفعال'}</span></div>
                        </div>
                    </Card>
                </div>
            )}

            {activeTab === 'settings' && (
                 <Card>
                    <div className="flex justify-between items-center mb-4">
                         <h3 className="text-lg font-semibold">پیکربندی دستگاه</h3>
                         <button onClick={handleLoadSettings} className="text-sm bg-gray-700 hover:bg-gray-600 py-1 px-3 rounded-lg">بارگذاری تنظیمات فعلی</button>
                    </div>
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                        {(Object.keys(settings).length > 0) ? (Object.keys(settings) as Array<keyof DeviceSettings>).map(key => (
                            <div key={key}>
                                <label className="block text-sm font-medium text-gray-300 mb-1 capitalize">{key.replace(/([A-Z])/g, ' $1')}:</label>
                                <input type={typeof settings[key] === 'number' ? 'number' : 'text'} name={key} value={settings[key] || ''} onChange={handleSettingsChange} className="w-full p-2 bg-gray-700 border border-gray-600 rounded-md focus:outline-none focus:ring-2 focus:ring-teal-500 text-white font-mono" />
                            </div>
                        )) : <p className="text-gray-400 md:col-span-2">برای ویرایش، ابتدا تنظیمات فعلی را بارگذاری کنید.</p>}
                    </div>
                    <div className="mt-6">
                         <button onClick={handleSaveSettings} disabled={Object.keys(settings).length === 0} className="bg-green-600 hover:bg-green-500 text-white font-bold py-2 px-4 rounded-lg transition-colors disabled:bg-gray-600 disabled:cursor-not-allowed">ذخیره تنظیمات روی دستگاه</button>
                    </div>
                </Card>
            )}

             {activeTab === 'firmware' && (
                <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                    <Card>
                         <h3 className="text-lg font-semibold mb-2">به‌روزرسانی سیستم‌عامل</h3>
                         <p className="text-gray-400 text-sm mb-4">سیستم‌عامل دستگاه را از طریق پورتال وب آن به‌روزرسانی کنید.</p>
                         <a href={`http://${ip}/update`} target="_blank" rel="noopener noreferrer" className="inline-block w-full text-center bg-teal-600 hover:bg-teal-500 text-white font-bold py-2 px-4 rounded-lg transition-colors">
                            رفتن به صفحه سیستم‌عامل
                         </a>
                    </Card>
                    <Card>
                         <h3 className="text-lg font-semibold mb-2">به‌روزرسانی رابط HTML</h3>
                         <p className="text-gray-400 text-sm mb-4">یک فایل HTML جدید برای سفارشی‌سازی رابط وب بارگذاری کنید.</p>
                         <form action={`http://${ip}/upload`} method="POST" encType="multipart/form-data" target="_blank">
                            <input type="file" name="htmlFile" accept=".html" className="mb-4 block w-full text-sm text-gray-300 file:me-4 file:py-2 file:px-4 file:rounded-full file:border-0 file:text-sm file:font-semibold file:bg-teal-500 file:text-white hover:file:bg-teal-600"/>
                            <button type="submit" className="w-full bg-teal-600 hover:bg-teal-500 text-white font-bold py-2 px-4 rounded-lg transition-colors">بارگذاری HTML</button>
                         </form>
                    </Card>
                </div>
             )}
        </div>
    );
};

export default DeviceControlPanel;