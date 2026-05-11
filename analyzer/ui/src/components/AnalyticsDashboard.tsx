import React, { useState, useEffect, useMemo, useCallback } from 'react';
import moment from 'moment-jalaali';
import { WorkSession, DeviceInfo, PressCounterRecord } from '../types';
import { fetchWorkSessions, fetchDeviceInfo, fetchSessionPresses } from '../services/api';
import Spinner from './ui/Spinner';
import Card from './ui/Card';
import ProductivityHeatmap from './charts/ProductivityHeatmap';
import DeviceHealthChart from './charts/DeviceHealthChart';
import PerformanceDistributionChart from './charts/PerformanceDistributionChart';
import AllPressIntervalsChart from './charts/AllPressIntervalsChart';
import IdleTimeChart from './charts/IdleTimeChart';
import { exportToExcel } from '../services/export';
import { formatDateTime, formatDurationForExport } from '../services/formatters';
import ChartContainer from './ui/ChartContainer';
import DurationHeatmap from './charts/DurationHeatmap';
import IntervalHeatmap from './charts/IntervalHeatmap';


const StatCard: React.FC<{ title: string; value: string; icon: React.ReactNode }> = ({ title, value, icon }) => (
    <Card className="flex-row items-center justify-between">
        <div>
            <p className="text-sm text-gray-400">{title}</p>
            <p className="text-2xl font-bold text-white">{value}</p>
        </div>
        <div className="text-teal-400 bg-gray-700 p-3 rounded-full">{icon}</div>
    </Card>
);

const formatDuration = (seconds: number) => {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    return `${h} ساعت ${m} دقیقه`;
};

const AnalyticsDashboard: React.FC<{ device: string | null }> = ({ device }) => {
    const [sessions, setSessions] = useState<WorkSession[]>([]);
    const [deviceInfo, setDeviceInfo] = useState<DeviceInfo[]>([]);
    const [allPressData, setAllPressData] = useState<PressCounterRecord[]>([]);
    const [loading, setLoading] = useState<boolean>(true);
    const [refreshing, setRefreshing] = useState<boolean>(false);
    const [error, setError] = useState<string | null>(null);
    const [selectedDie, setSelectedDie] = useState<string>('all');

    const isAllDevicesView = device === null;
    
    const JALALI_FORMAT = 'jYYYY/jMM/jDD HH:mm';
    
    const sevenDaysAgoJalali = moment().subtract(7, 'days').format(JALALI_FORMAT);
    const nowJalali = moment().format(JALALI_FORMAT);

    const [idleTimeRangeJalali, setIdleTimeRangeJalali] = useState({
        start: sevenDaysAgoJalali,
        end: nowJalali
    });

    const [intervalTimeRangeJalali, setIntervalTimeRangeJalali] = useState({
        start: sevenDaysAgoJalali,
        end: nowJalali
    });

    const handleIdleTimeRangeChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        setIdleTimeRangeJalali(prev => ({ ...prev, [e.target.name]: e.target.value }));
    };

    const handleIntervalTimeRangeChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        setIntervalTimeRangeJalali(prev => ({ ...prev, [e.target.name]: e.target.value }));
    };


    const loadAnalyticsData = useCallback(async (isRefresh = false) => {
        try {
            if (isRefresh) {
                setRefreshing(true);
            } else {
                setLoading(true);
            }
            setError(null);
            
            const sessionData = await fetchWorkSessions(device, {});
            sessionData.sessions.sort((a, b) => new Date(b.start_time).getTime() - new Date(a.start_time).getTime());
            setSessions(sessionData.sessions);
    
            // Fetch press data for all sessions, for both single and all devices view
            if (sessionData.sessions.length > 0) {
                const pressDataPromises = sessionData.sessions.map(s =>
                    fetchSessionPresses(s.device, s.start_time, s.end_time)
                );
                const pressDataResults = await Promise.all(pressDataPromises);
                const combinedPressData = pressDataResults.flatMap(result => result.records);
                combinedPressData.sort((a, b) => new Date(a.timestamp).getTime() - new Date(b.timestamp).getTime());
                setAllPressData(combinedPressData);
            } else {
                setAllPressData([]);
            }

            // Conditional fetching for device-specific info
            if (device) {
                const deviceInfoData = await fetchDeviceInfo(device);
                setDeviceInfo(deviceInfoData.records);
            } else {
                // Reset data that depends on a single device
                setDeviceInfo([]);
            }
        } catch (err: any) {
            setError(err.message || 'خطا در بارگذاری داده‌های تحلیلی.');
        } finally {
            setLoading(false);
            setRefreshing(false);
        }
    }, [device]);

    useEffect(() => {
        loadAnalyticsData();
    }, [loadAnalyticsData]);

    const dieOptions = useMemo(() => {
        if (!sessions || sessions.length === 0) return [];
        const allDies = [...new Set(sessions.map(s => s.die_id).filter(Boolean))];
        return allDies.sort();
    }, [sessions]);

    const summaryStats = useMemo(() => {
        if (sessions.length === 0) {
            return { totalSessions: 0, totalPresses: 0, avgDuration: 0, avgTemp: 0 };
        }
        const totalPresses = sessions.reduce((sum, s) => sum + s.total_count, 0);
        const totalDuration = sessions.reduce((sum, s) => sum + (s.duration_seconds || 0), 0);
        const totalTemp = sessions.reduce((sum, s) => sum + s.average_temperature, 0);
        
        return {
            totalSessions: sessions.length,
            totalPresses,
            avgDuration: totalDuration / sessions.length,
            avgTemp: totalTemp / sessions.length,
        }
    }, [sessions]);

    const handleExport = () => {
        const dataToExport = sessions.map(s => ({
            'شناسه جلسه': s.session_id,
            'دستگاه': s.device,
            'شناسه پرسنل': s.personnel_id,
            'شناسه قالب': s.die_id || 'N/A',
            'زمان شروع': formatDateTime(s.start_time),
            'زمان پایان': s.end_time ? formatDateTime(s.end_time) : 'در حال انجام',
            'مدت زمان': formatDurationForExport(s.duration_seconds),
            'تعداد کل': s.total_count,
            'تعداد ضربات ناموفق': s.missing_counts.length,
            'شمارش‌های از دست رفته': s.missing_counts.join(', '),
            'میانگین دمای': s.average_temperature.toFixed(1),
            'میانگین فاصله ضرب': s.average_press_interval_seconds.toFixed(2),
            'میانگین زمان ضرب': s.average_press_duration_ms.toFixed(0),
            'دلیل پایان': s.end_reason || 'تکمیل',
        }));
        exportToExcel([{ name: 'Sessions', data: dataToExport }], `گزارش-تحلیلی-${device || 'همه-دستگاه‌ها'}`);
    };


    if (loading) {
        return <div className="flex justify-center items-center h-96"><Spinner /></div>;
    }

    if (error) {
        return <div className="bg-red-900/50 text-red-300 p-4 rounded-lg">{error}</div>;
    }

    const parseJalaliToISO = (jalaliStr: string): string | null => {
        const m = moment(jalaliStr, JALALI_FORMAT, true); // strict mode
        return m.isValid() ? m.toISOString() : null;
    };

    const idleRangeForChart = {
        start: parseJalaliToISO(idleTimeRangeJalali.start) ?? new Date(0).toISOString(),
        end: parseJalaliToISO(idleTimeRangeJalali.end) ?? new Date().toISOString(),
    };
    
    const intervalRangeForChart = {
        start: parseJalaliToISO(intervalTimeRangeJalali.start),
        end: parseJalaliToISO(intervalTimeRangeJalali.end),
    };

    const idleTimeControls = (
        <div className="flex items-center gap-2 text-sm">
            <input type="text" name="start" value={idleTimeRangeJalali.start} onChange={handleIdleTimeRangeChange} placeholder="مثال: 1403/05/10 08:30" dir="ltr" className="bg-gray-700 border border-gray-600 rounded-md p-2 text-white focus:outline-none focus:ring-2 focus:ring-teal-500 w-full" />
            <span className="text-gray-400">تا</span>
            <input type="text" name="end" value={idleTimeRangeJalali.end} onChange={handleIdleTimeRangeChange} placeholder="مثال: 1403/05/17 17:00" dir="ltr" className="bg-gray-700 border border-gray-600 rounded-md p-2 text-white focus:outline-none focus:ring-2 focus:ring-teal-500 w-full" />
        </div>
    );

    const intervalChartControls = (
        <div className="flex items-center gap-2 text-sm">
            <input type="text" name="start" value={intervalTimeRangeJalali.start} onChange={handleIntervalTimeRangeChange} placeholder="مثال: 1403/05/10 08:30" dir="ltr" className="bg-gray-700 border border-gray-600 rounded-md p-2 text-white focus:outline-none focus:ring-2 focus:ring-teal-500 w-full" />
            <span className="text-gray-400">تا</span>
            <input type="text" name="end" value={intervalTimeRangeJalali.end} onChange={handleIntervalTimeRangeChange} placeholder="مثال: 1403/05/17 17:00" dir="ltr" className="bg-gray-700 border border-gray-600 rounded-md p-2 text-white focus:outline-none focus:ring-2 focus:ring-teal-500 w-full" />
        </div>
    );


    return (
        <div className="space-y-8">
            <div className="flex justify-between items-start">
                <div>
                    <h2 className="text-2xl font-bold text-white">نمای کلی تحلیل‌ها</h2>
                    <p className="text-gray-400">دستگاه: <span className="text-teal-400 font-semibold">{device || 'همه دستگاه‌ها'}</span></p>
                </div>
                <div className="flex items-center gap-2">
                     <button 
                        onClick={handleExport}
                        className="p-2 text-gray-300 bg-gray-800 hover:bg-gray-700 rounded-lg transition-colors"
                        title="خروجی اکسل"
                    >
                         <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" /></svg>
                    </button>
                    <button 
                        onClick={() => loadAnalyticsData(true)}
                        disabled={refreshing}
                        className="p-2 text-gray-300 bg-gray-800 hover:bg-gray-700 rounded-lg transition-colors disabled:opacity-50 disabled:cursor-wait"
                        title="به‌روزرسانی"
                    >
                        <svg xmlns="http://www.w3.org/2000/svg" className={`h-5 w-5 ${refreshing ? 'animate-spin' : ''}`} fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h5M20 20v-5h-5M20 4s-1.5-2-5-2-7 3-7 7 3 7 7 7 5-2 5-2" />
                        </svg>
                    </button>
                </div>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-4 gap-6">
                <StatCard title="تعداد کل جلسات" value={summaryStats.totalSessions.toLocaleString()} icon={<svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 17v-2m3 2v-4m3 4v-6m2 10H7a2 2 0 01-2-2V7a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" /></svg>} />
                <StatCard title="تعداد کل ضربات" value={summaryStats.totalPresses.toLocaleString()} icon={<svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 3v2m6-2v2M9 19v2m6-2v2M5 9H3m2 6H3m18-6h-2m-2 6h-2M12 6V3m0 18v-3" /></svg>} />
                <StatCard title="میانگین زمان جلسه" value={formatDuration(summaryStats.avgDuration)} icon={<svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z" /></svg>} />
                <StatCard title="میانگین دما" value={`${summaryStats.avgTemp.toFixed(1)} °C`} icon={<svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 10V3L4 14h7v7l9-11h-7z" /></svg>} />
            </div>
            
            <div className="space-y-4">
                <div className="flex flex-wrap justify-between items-center p-4 border-b border-gray-700">
                    <h3 className="text-xl font-bold text-white">تحلیل‌های نقشه حرارتی</h3>
                     {dieOptions.length > 0 && (
                        <div className="flex items-center mt-2 sm:mt-0">
                            <label htmlFor="die-selector-global" className="text-sm text-gray-400 me-2 whitespace-nowrap">فیلتر قالب:</label>
                            <select
                                id="die-selector-global"
                                value={selectedDie}
                                onChange={(e) => setSelectedDie(e.target.value)}
                                className="bg-gray-700 border border-gray-600 rounded-md p-2 text-sm text-white focus:outline-none focus:ring-1 focus:ring-teal-500"
                            >
                                <option value="all">همه قالب‌ها</option>
                                {dieOptions.map(die => <option key={die} value={die as string}>{die}</option>)}
                            </select>
                        </div>
                    )}
                </div>
                <div className="grid grid-cols-1 gap-8">
                    <ChartContainer title="بهره‌وری (تعداد ضربات)" className="h-[40vh]">
                        <ProductivityHeatmap sessions={sessions} selectedDie={selectedDie} />
                    </ChartContainer>
                    <ChartContainer title="مدت زمان جلسات" className="h-[40vh]">
                        <DurationHeatmap sessions={sessions} selectedDie={selectedDie} />
                    </ChartContainer>
                    <ChartContainer title="میانگین فاصله ضربات" className="h-[40vh]">
                        <IntervalHeatmap sessions={sessions} selectedDie={selectedDie} />
                    </ChartContainer>
                </div>
            </div>

            <div className="space-y-4">
                <div className="p-4 border-b border-gray-700">
                    <h3 className="text-xl font-bold text-white">تحلیل‌های دیگر</h3>
                </div>
                <div className="grid grid-cols-1 xl:grid-cols-2 gap-8">
                    {!isAllDevicesView && (
                        <ChartContainer title="نمودار سلامت دستگاه" className="h-[40vh]">
                            <DeviceHealthChart sessions={sessions} deviceInfo={deviceInfo} />
                        </ChartContainer>
                    )}
                    <ChartContainer title="توزیع عملکرد جلسات" className={`h-[40vh] ${isAllDevicesView ? 'xl:col-span-2' : ''}`}>
                        <PerformanceDistributionChart sessions={sessions} />
                    </ChartContainer>
                    <ChartContainer
                        title="سری زمانی فواصل بین ضربات (کلی)"
                        headerControls={intervalChartControls}
                        className="xl:col-span-2 h-[40vh]"
                    >
                        <AllPressIntervalsChart pressData={allPressData} sessions={sessions} filterRange={intervalRangeForChart} />
                    </ChartContainer>
                    <ChartContainer 
                        title="تحلیل زمان بیکاری" 
                        headerControls={idleTimeControls} 
                        className="xl:col-span-2 h-[40vh]"
                    >
                        <IdleTimeChart sessions={sessions} startDate={idleRangeForChart.start} endDate={idleRangeForChart.end} />
                    </ChartContainer>
                </div>
            </div>
        </div>
    );
};

export default AnalyticsDashboard;