import React, { useState, useEffect, useMemo } from 'react';
import { fetchSessionPresses } from '../services/api';
import { PressCounterRecord, WorkSession } from '../types';
import Modal from './ui/Modal';
import PressDataChart from './charts/PressDataChart';
import PressIntervalChart from './charts/PressIntervalChart';
import Spinner from './ui/Spinner';
import { formatDateTime } from '../services/formatters';

const StatItem: React.FC<{ label: string; value: string | number; unit?: string; }> = ({ label, value, unit }) => (
    <div className="bg-gray-700/50 p-3 rounded-lg text-center">
        <p className="text-sm text-gray-400">{label}</p>
        <p className="text-xl font-bold text-teal-300">
            {value}
            {unit && <span className="text-base font-normal text-gray-300 me-1">{unit}</span>}
        </p>
    </div>
);

const DetailRow: React.FC<{ label: string, children: React.ReactNode }> = ({ label, children }) => (
    <div className="py-2 grid grid-cols-3 gap-4">
        <dt className="text-sm font-medium text-gray-400">{label}</dt>
        <dd className="text-sm text-gray-200 col-span-2 font-mono">{children}</dd>
    </div>
);

const formatEndReason = (reason: string | null | undefined): string => {
    if (!reason) return 'تکمیل عادی';

    switch(reason) {
        case 'nfc_event':
            return 'رویداد NFC (خروج با تگ)';
        case 'device_status':
            return 'تغییر وضعیت دستگاه';
        case 'timeout':
            return 'پایان زمان جلسه';
        default:
            return reason;
    }
}

const TabButton: React.FC<{ active: boolean; onClick: () => void; children: React.ReactNode }> = ({ active, onClick, children }) => (
    <button
      onClick={onClick}
      className={`px-4 py-2 text-sm font-medium rounded-t-lg transition-colors focus:outline-none ${
        active
          ? 'bg-gray-800 border-gray-700 border-t border-l border-r text-white'
          : 'text-gray-400 hover:bg-gray-700/50'
      }`}
    >
      {children}
    </button>
);


const SessionDetailModal: React.FC<{ sessions: WorkSession[]; onClose: () => void; }> = ({ sessions, onClose }) => {
    const [pressData, setPressData] = useState<PressCounterRecord[]>([]);
    const [loading, setLoading] = useState<boolean>(true);
    const [error, setError] = useState<string | null>(null);
    const [activeTab, setActiveTab] = useState<'summary' | 'performance' | 'intervals'>('performance');

    const isCombinedView = sessions.length > 1;

    useEffect(() => {
        const getPressData = async () => {
            if (!sessions || sessions.length === 0) return;
            try {
                setLoading(true);
                setError(null);
                const allPressDataPromises = sessions.map(s => fetchSessionPresses(s.device, s.start_time, s.end_time));
                const allPressData = await Promise.all(allPressDataPromises);

                const combinedRecords = allPressData.flatMap(list => list.records);
                combinedRecords.sort((a, b) => new Date(a.timestamp).getTime() - new Date(b.timestamp).getTime());
                
                setPressData(combinedRecords);
            } catch (err: any) {
                setError(err.message || "خطا در بارگذاری جزئیات جلسه.");
            } finally {
                setLoading(false);
            }
        };

        getPressData();
    }, [sessions]);

    const aggregatedStats = useMemo(() => {
        if (!sessions || sessions.length === 0) return null;

        if (!isCombinedView) {
            return sessions[0];
        }

        const totalCount = sessions.reduce((sum, s) => sum + s.total_count, 0);
        const totalDuration = sessions.reduce((sum, s) => sum + (s.duration_seconds || 0), 0);
        const allMissingCounts = sessions.flatMap(s => s.missing_counts);
        
        const earliestStartTime = sessions.reduce((earliest, s) => 
            new Date(s.start_time) < new Date(earliest) ? s.start_time : earliest
        , sessions[0].start_time);

        const latestEndTime = sessions.reduce((latest, s) => {
            if (!s.end_time) return latest;
            if (!latest) return s.end_time;
            return new Date(s.end_time) > new Date(latest) ? s.end_time : latest;
        }, sessions.find(s => s.end_time)?.end_time || null);


        let avgTemp = 0, avgDurationMs = 0, avgInterval = 0;
        
        if(pressData.length > 0) {
            avgTemp = pressData.reduce((sum, r) => sum + r.temperature, 0) / pressData.length;
            avgDurationMs = pressData.reduce((sum, r) => sum + r.duration, 0) / pressData.length;

            if (pressData.length > 1) {
                const intervals = [];
                for (let i = 1; i < pressData.length; i++) {
                    const interval = (new Date(pressData[i].timestamp).getTime() - new Date(pressData[i-1].timestamp).getTime()) / 1000;
                    intervals.push(interval);
                }
                avgInterval = intervals.reduce((sum, i) => sum + i, 0) / intervals.length;
            }
        }
        
        return {
            personnel_id: 'چند نفر',
            total_count: totalCount,
            average_press_interval_seconds: avgInterval,
            average_temperature: avgTemp,
            average_press_duration_ms: avgDurationMs,
            missing_counts: allMissingCounts,
            start_time: earliestStartTime,
            end_time: latestEndTime,
            duration_seconds: totalDuration,
            device: [...new Set(sessions.map(s => s.device))].join(', '),
            die_id: [...new Set(sessions.map(s => s.die_id).filter(Boolean))].join(', ') || 'N/A',
            end_reason: 'چندگانه',
            session_id: 'نمای ترکیبی'
        };

    }, [sessions, pressData, isCombinedView]);

    const title = isCombinedView 
        ? `نمای ترکیبی (${sessions.length} جلسه)` 
        : `جلسه: ${formatDateTime(sessions[0].start_time)}`;
    
    const renderContent = () => {
        if (loading || !aggregatedStats) {
            return <div className="flex justify-center items-center h-96"><Spinner /></div>
        }

        if (error) {
            return <div className="flex justify-center items-center h-full bg-red-900/50 text-red-300 p-4 rounded-lg">{error}</div>
        }
        
        return (
            <>
                <div className="border-b border-gray-700 flex space-x-1 -mt-6 -mx-6 px-6 mb-4">
                    <TabButton active={activeTab === 'performance'} onClick={() => setActiveTab('performance')}>نمودار عملکرد</TabButton>
                    <TabButton active={activeTab === 'intervals'} onClick={() => setActiveTab('intervals')}>تحلیل فواصل</TabButton>
                    <TabButton active={activeTab === 'summary'} onClick={() => setActiveTab('summary')}>خلاصه</TabButton>
                </div>

                <div className="bg-gray-800 p-4 rounded-b-lg -mt-4">
                    {activeTab === 'performance' && (
                        <div className="h-[500px]">
                            <PressDataChart data={pressData} />
                        </div>
                    )}
                    {activeTab === 'intervals' && (
                         <div className="h-[500px]">
                             <PressIntervalChart 
                                data={pressData} 
                                averageInterval={aggregatedStats.average_press_interval_seconds} 
                            />
                         </div>
                    )}
                    {activeTab === 'summary' && (
                        <div>
                            <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mb-6">
                                <StatItem label={isCombinedView ? "پرسنل" : "پرسنل"} value={aggregatedStats.personnel_id} />
                                <StatItem label="کل ضربات" value={aggregatedStats.total_count.toLocaleString()} />
                                <StatItem label="میانگین فاصله" value={aggregatedStats.average_press_interval_seconds.toFixed(2)} unit="ثانیه" />
                                <StatItem label="میانگین دما" value={aggregatedStats.average_temperature.toFixed(1)} unit="°C" />
                                <StatItem label="میانگین زمان ضرب" value={aggregatedStats.average_press_duration_ms.toFixed(0)} unit="میلی‌ثانیه" />
                            </div>

                            {aggregatedStats.missing_counts.length > 0 && (
                                <div className="mb-6 p-3 bg-yellow-900/50 text-yellow-300 rounded-lg text-sm">
                                    <strong>هشدار:</strong> {aggregatedStats.missing_counts.length} شمارش ضربه از دست رفته شناسایی شد.
                                </div>
                            )}

                            <h4 className="text-lg font-semibold text-white mb-4">جزئیات جلسه</h4>
                             <div className="bg-gray-900/50 p-4 rounded-lg">
                                <dl className="divide-y divide-gray-700">
                                    <DetailRow label="دستگاه">{aggregatedStats.device}</DetailRow>
                                    <DetailRow label="شناسه قالب (ها)">{aggregatedStats.die_id}</DetailRow>
                                    <DetailRow label="زمان شروع">{formatDateTime(aggregatedStats.start_time)}</DetailRow>
                                    <DetailRow label="زمان پایان">{aggregatedStats.end_time ? formatDateTime(aggregatedStats.end_time) : 'در حال انجام'}</DetailRow>
                                    <DetailRow label="مدت زمان کل">
                                        {Math.floor((aggregatedStats.duration_seconds || 0) / 3600)} ساعت {Math.floor(((aggregatedStats.duration_seconds || 0) % 3600) / 60)} دقیقه {Math.floor((aggregatedStats.duration_seconds || 0) % 60)} ثانیه
                                    </DetailRow>
                                    <DetailRow label="دلیل پایان">
                                        {isCombinedView 
                                            ? 'چندگانه / متغیر' 
                                            : aggregatedStats.end_time 
                                                ? formatEndReason(aggregatedStats.end_reason) 
                                                : 'در حال انجام'
                                        }
                                    </DetailRow>
                                    <DetailRow label="شناسه جلسه (ها)">
                                        {isCombinedView 
                                            ? `${sessions.length} جلسه ترکیب شده` 
                                            : aggregatedStats.session_id
                                        }
                                    </DetailRow>
                                </dl>
                             </div>
                        </div>
                    )}
                </div>
            </>
        )
    }

    return (
        <Modal onClose={onClose} title={title}>
            {renderContent()}
        </Modal>
    );
};

export default SessionDetailModal;