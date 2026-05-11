import React, { useState, useEffect, useCallback } from 'react';
import { fetchWorkSessions, fetchSessionPresses } from '../services/api';
import { WorkSession } from '../types';
import SessionCard from './SessionCard';
import SessionDetailModal from './SessionDetailModal';
import Spinner from './ui/Spinner';
import { exportToExcel } from '../services/export';
import { formatDateTime, formatDurationForExport, formatForSheetName } from '../services/formatters';

type Filter = { type: 'days'; value: number | undefined };

const dayOptions: { label: string; filter: Filter }[] = [
    { label: 'روز گذشته', filter: { type: 'days', value: 1 } },
    { label: '۷ روز گذشته', filter: { type: 'days', value: 7 } },
    { label: '۳۰ روز گذشته', filter: { type: 'days', value: 30 } },
    { label: 'همه', filter: { type: 'days', value: undefined } },
];

const SessionList: React.FC<{ device: string | null }> = ({ device }) => {
    const [sessions, setSessions] = useState<WorkSession[]>([]);
    const [loading, setLoading] = useState<boolean>(true);
    const [refreshing, setRefreshing] = useState<boolean>(false);
    const [error, setError] = useState<string | null>(null);
    const [filter, setFilter] = useState<Filter>(dayOptions[0].filter);
    
    const [selectedSessions, setSelectedSessions] = useState<WorkSession[]>([]);
    const [sessionsForModal, setSessionsForModal] = useState<WorkSession[]>([]);
    const [exporting, setExporting] = useState<boolean>(false);

    const getSessions = useCallback(async (isRefresh = false) => {
        try {
            if (isRefresh) {
                setRefreshing(true);
            } else {
                setLoading(true);
            }
            setError(null);
            if (!isRefresh) {
                setSelectedSessions([]); // Clear selection on new data fetch, but not on refresh
            }
            
            let apiFilter: { days?: number; } = {};
            if (filter.value !== undefined) {
              apiFilter.days = filter.value;
            }

            const data = await fetchWorkSessions(device, apiFilter);
            data.sessions.sort((a, b) => new Date(b.start_time).getTime() - new Date(a.start_time).getTime());
            setSessions(data.sessions);
        } catch (err: any) {
            setError(err.message || 'خطا در دریافت جلسات.');
        } finally {
            setLoading(false);
            setRefreshing(false);
        }
    }, [device, filter]);

    useEffect(() => {
        getSessions();
    }, [getSessions]);

    const handleToggleSelection = (session: WorkSession) => {
        setSelectedSessions(prev => {
            if (prev.find(s => s.session_id === session.session_id)) {
                return prev.filter(s => s.session_id !== session.session_id);
            } else {
                return [...prev, session];
            }
        });
    };

    const handleExport = async () => {
        if (exporting || sessions.length === 0) return;
        setExporting(true);
        setError(null);
        try {
            const summaryData = sessions.map(s => ({
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
    
            const sheets = [{ name: 'خلاصه جلسات', data: summaryData }];
    
            const pressDataPromises = sessions.map(session => 
                fetchSessionPresses(session.device, session.start_time, session.end_time)
                    .then(pressList => ({
                        session: session, // Pass session object for context
                        records: pressList.records
                    }))
            );
            
            const allPressData = await Promise.all(pressDataPromises);
    
            for (const sessionPressData of allPressData) {
                if (sessionPressData.records && sessionPressData.records.length > 0) {
                    const pressDetails = sessionPressData.records.map(p => ({
                        'برچسب زمانی': formatDateTime(p.timestamp),
                        'تعداد': p.count,
                        'زمان ضرب (ms)': p.duration,
                        'دما (C)': p.temperature.toFixed(1),
                    }));
                    const session = sessionPressData.session;
                    // Use a safe and descriptive name for the sheet
                    const sheetName = `پرس ${session.personnel_id} ${formatForSheetName(session.start_time)}`;
                    sheets.push({ name: sheetName, data: pressDetails });
                }
            }
    
            exportToExcel(sheets, `گزارش-جلسات-${device || 'همه-دستگاه‌ها'}`);
        } catch (err: any) {
            setError(err.message || 'خطا در آماده‌سازی داده برای خروجی اکسل.');
        } finally {
            setExporting(false);
        }
    };

    const renderContent = () => {
        if (loading) {
            return <div className="flex justify-center items-center h-96"><Spinner /></div>;
        }
        if (error) {
            return <div className="bg-red-900/50 text-red-300 p-4 rounded-lg">{error}</div>;
        }
        if (sessions.length === 0) {
            return <div className="text-center text-gray-400 p-8">هیچ جلسه کاری در این بازه زمانی یافت نشد.</div>;
        }
        return (
            <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-6">
                {sessions.map(session => (
                    <SessionCard 
                        key={session.session_id} 
                        session={session} 
                        onSelect={() => handleToggleSelection(session)}
                        onViewDetails={() => setSessionsForModal([session])}
                        isSelected={!!selectedSessions.find(s => s.session_id === session.session_id)}
                    />
                ))}
            </div>
        );
    };

    return (
        <div>
            <div className="flex flex-wrap justify-between items-center mb-6 gap-4">
                <div className="flex-grow">
                    <h2 className="text-2xl font-bold text-white">جلسات کاری برای <span className="text-teal-400">{device || 'همه دستگاه‌ها'}</span></h2>
                    {selectedSessions.length > 0 && (
                        <p className="text-sm text-gray-400 mt-1">{selectedSessions.length} جلسه انتخاب شده.</p>
                    )}
                </div>
                <div className="flex items-center gap-2">
                    {selectedSessions.length > 1 && (
                        <button 
                            onClick={() => setSessionsForModal([...selectedSessions])}
                            className="bg-teal-600 text-white font-bold py-2 px-4 rounded-lg hover:bg-teal-500 transition-colors order-first sm:order-none"
                        >
                            ترکیب {selectedSessions.length} مورد
                        </button>
                    )}
                    <button 
                        onClick={handleExport}
                        disabled={exporting}
                        className="bg-green-600 text-white font-bold py-2 px-4 rounded-lg hover:bg-green-500 transition-colors flex items-center gap-2 disabled:bg-gray-500 disabled:cursor-wait"
                        title="خروجی اکسل"
                    >
                         <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" /></svg>
                        <span>{exporting ? 'در حال آماده‌سازی...' : 'اکسل'}</span>
                    </button>
                </div>
                <div className="flex items-center space-x-2 bg-gray-800 rounded-lg p-1">
                    {dayOptions.map(option => (
                        <button 
                            key={option.label}
                            onClick={() => {
                                setFilter(option.filter);
                            }}
                            className={`px-4 py-2 text-sm font-medium rounded-md transition-colors ${JSON.stringify(filter) === JSON.stringify(option.filter) ? 'bg-teal-600 text-white' : 'text-gray-300 hover:bg-gray-700'}`}
                        >
                            {option.label}
                        </button>
                    ))}
                     <button 
                        onClick={() => getSessions(true)}
                        disabled={refreshing}
                        className="p-2 text-gray-300 hover:bg-gray-700 rounded-md transition-colors disabled:opacity-50 disabled:cursor-wait"
                        title="به‌روزرسانی"
                    >
                         <svg xmlns="http://www.w3.org/2000/svg" className={`h-5 w-5 ${refreshing ? 'animate-spin' : ''}`} fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h5M20 20v-5h-5M20 4s-1.5-2-5-2-7 3-7 7 3 7 7 7 5-2 5-2" />
                        </svg>
                    </button>
                </div>
            </div>

            {renderContent()}
            {sessionsForModal.length > 0 && (
                <SessionDetailModal sessions={sessionsForModal} onClose={() => setSessionsForModal([])} />
            )}
        </div>
    );
};

export default SessionList;