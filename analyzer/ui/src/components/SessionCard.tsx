import React from 'react';
import { WorkSession } from '../types';
import Card from './ui/Card';
import { formatDateTime, formatDuration } from '../services/formatters';

const getStatus = (session: WorkSession): { text: string; color: string; } => {
    if (!session.end_time) {
        return { text: "در حال انجام", color: "bg-green-500" };
    }
    switch (session.end_reason) {
        case 'timeout':
            return { text: "پایان (تایم اوت)", color: "bg-yellow-500" };
        case 'nfc_event':
            return { text: "پایان (NFC)", color: "bg-sky-500" };
        case 'device_status':
            return { text: "پایان (دستگاه)", color: "bg-orange-500" };
        default:
            return { text: "تکمیل شده", color: "bg-blue-500" };
    }
};

const Stat: React.FC<{ icon: React.ReactNode; label: string; value: string | number | undefined; unit?: string; }> = ({ icon, label, value, unit }) => (
    <div className="flex items-center text-sm text-gray-300">
        <span className="ms-2 text-gray-400">{icon}</span>
        <span className="font-semibold">{value ?? 'N/A'}</span>
        {unit && <span className="me-1 text-gray-400">{unit}</span>}
    </div>
);

interface SessionCardProps {
    session: WorkSession;
    onSelect: () => void;
    onViewDetails: () => void;
    isSelected: boolean;
}

const SessionCard: React.FC<SessionCardProps> = ({ session, onSelect, onViewDetails, isSelected }) => {
    const status = getStatus(session);
    
    return (
        <Card onClick={onSelect} className={isSelected ? 'border-teal-500 ring-2 ring-teal-500/50' : ''}>
            <div className="flex justify-between items-start">
                <div>
                    <p className="font-bold text-lg text-white">{session.personnel_id}</p>
                    <p className="text-xs text-gray-400">شناسه قالب: {session.die_id || 'N/A'}</p>
                </div>
                <div className={`flex items-center text-xs font-bold text-white px-2 py-1 rounded-full ${status.color}`}>
                    <div className="w-2 h-2 rounded-full bg-white ms-1.5"></div>
                    {status.text}
                </div>
            </div>
            
            <div className="my-4 border-t border-gray-700"></div>

            <div className="space-y-3">
                <Stat 
                    icon={<svg xmlns="http://www.w3.org/2000/svg" className="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 7V3m8 4V3m-9 8h10M5 21h14a2 2 0 002-2V7a2 2 0 00-2-2H5a2 2 0 00-2 2v12a2 2 0 002 2z" /></svg>}
                    label="شروع"
                    value={formatDateTime(session.start_time)}
                />
                <Stat 
                    icon={<svg xmlns="http://www.w3.org/2000/svg" className="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z" /></svg>}
                    label="مدت زمان"
                    value={formatDuration(session.duration_seconds)}
                />
                <Stat 
                    icon={<svg xmlns="http://www.w3.org/2000/svg" className="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 3v2m6-2v2M9 19v2m6-2v2M5 9H3m2 6H3m18-6h-2m-2 6h-2M12 6V3m0 18v-3" /></svg>}
                    label="تعداد کل ضربات"
                    value={session.total_count}
                    unit="ضربه"
                />
            </div>

            <div className="mt-4 pt-4 border-t border-gray-700 text-left">
                <button 
                    onClick={(e) => {
                        e.stopPropagation(); // Prevent card selection click
                        onViewDetails();
                    }}
                    className="text-sm font-semibold text-teal-400 hover:text-teal-300 transition-colors"
                >
                    مشاهده جزئیات &larr;
                </button>
            </div>
        </Card>
    );
};

export default SessionCard;