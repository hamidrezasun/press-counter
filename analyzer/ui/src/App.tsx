import React, { useState, useEffect } from 'react';
import DeviceSelector from './components/DeviceSelector';
import SessionList from './components/SessionList';
import DeviceInfoPanel from './components/DeviceInfoPanel';
import AnalyticsDashboard from './components/AnalyticsDashboard';
import { DeviceInfo } from './types';
import { fetchDeviceInfo } from './services/api';
import DeviceControlPanel from './components/DeviceControlPanel';

const App: React.FC = () => {
    const [selectedDevice, setSelectedDevice] = useState<string | null>(null);
    const [view, setView] = useState<'sessions' | 'analytics' | 'control'>('sessions');
    const [deviceInfo, setDeviceInfo] = useState<DeviceInfo | null>(null);
    const [deviceInfoLoading, setDeviceInfoLoading] = useState<boolean>(false);
    const [deviceInfoError, setDeviceInfoError] = useState<string | null>(null);


    useEffect(() => {
        if (!selectedDevice) {
            setDeviceInfo(null);
            return;
        };

        const getInfo = async () => {
            try {
                setDeviceInfoLoading(true);
                setDeviceInfoError(null);
                const data = await fetchDeviceInfo(selectedDevice);
                if (data.records && data.records.length > 0) {
                    const sortedRecords = [...data.records].sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime());
                    setDeviceInfo(sortedRecords[0]);
                } else {
                    setDeviceInfo(null);
                }
            } catch (err: any) {
                setDeviceInfoError(err.message || 'خطا در دریافت اطلاعات دستگاه.');
            } finally {
                setDeviceInfoLoading(false);
            }
        };
        getInfo();
    }, [selectedDevice]);


    return (
        <div className="min-h-screen bg-gray-900 text-gray-100 font-sans">
            <header className="bg-gray-800/50 backdrop-blur-sm sticky top-0 z-20 shadow-lg border-b border-gray-700">
                <div className="container mx-auto flex justify-between items-center p-4">
                    <h1 className="text-2xl font-bold text-teal-400 tracking-wider">
                        مانیتور پرس
                    </h1>
                </div>
            </header>
            
            <main className="container mx-auto p-4 md:p-6 lg:p-8">
                <div className="grid grid-cols-1 lg:grid-cols-4 gap-8">
                    <aside className="lg:col-span-1 bg-gray-800 p-6 rounded-lg shadow-xl self-start">
                        <DeviceSelector 
                            onDeviceSelect={(device) => {
                                setSelectedDevice(device);
                                setView('sessions'); // Reset view on new device selection
                            }} 
                            selectedDevice={selectedDevice} 
                        />
                        {selectedDevice && (
                           <DeviceInfoPanel 
                                deviceInfo={deviceInfo}
                                loading={deviceInfoLoading}
                                error={deviceInfoError}
                           />
                        )}
                    </aside>
                    
                    <section className="lg:col-span-3">
                        <div>
                            <div className="mb-6 border-b border-gray-700 flex space-x-2">
                                <button 
                                    onClick={() => setView('sessions')}
                                    className={`px-4 py-2 text-sm font-medium transition-colors focus:outline-none ${view === 'sessions' ? 'border-b-2 border-teal-500 text-white' : 'text-gray-400 hover:text-white'}`}
                                >
                                    جلسات کاری
                                </button>
                                <button 
                                    onClick={() => setView('analytics')}
                                    className={`px-4 py-2 text-sm font-medium transition-colors focus:outline-none ${view === 'analytics' ? 'border-b-2 border-teal-500 text-white' : 'text-gray-400 hover:text-white'}`}
                                >
                                    تحلیل‌ها
                                </button>
                                <button 
                                    onClick={() => setView('control')}
                                    disabled={!selectedDevice || !deviceInfo || !deviceInfo.ip}
                                    className={`px-4 py-2 text-sm font-medium transition-colors focus:outline-none ${view === 'control' ? 'border-b-2 border-teal-500 text-white' : 'text-gray-400 hover:text-white'} disabled:text-gray-600 disabled:border-transparent disabled:cursor-not-allowed`}
                                    title={!selectedDevice ? "برای کنترل، یک دستگاه انتخاب کنید" : (!deviceInfo || !deviceInfo.ip ? "آدرس IP دستگاه موجود نیست" : "کنترل دستگاه")}
                                >
                                    کنترل
                                </button>
                            </div>
                            {view === 'sessions' && <SessionList key={`${selectedDevice || 'all'}-sessions`} device={selectedDevice} />}
                            {view === 'analytics' && <AnalyticsDashboard key={`${selectedDevice || 'all'}-analytics`} device={selectedDevice} />}
                            {view === 'control' && selectedDevice && <DeviceControlPanel key={`${selectedDevice}-control`} deviceInfo={deviceInfo} />}
                            {view === 'control' && !selectedDevice && (
                                <div className="flex items-center justify-center h-96 bg-gray-800 rounded-lg shadow-xl border-2 border-dashed border-gray-700">
                                    <p className="text-xl text-gray-400">برای دسترسی به کنترل پنل، لطفا یک دستگاه انتخاب کنید.</p>
                                </div>
                            )}
                        </div>
                    </section>
                </div>
            </main>

            <footer className="text-center p-6 mt-8 text-gray-500 text-sm border-t border-gray-800">
                <p>&copy; {new Date().getFullYear()} Industrial IoT Solutions. All rights reserved.</p>
            </footer>
        </div>
    );
};

export default App;