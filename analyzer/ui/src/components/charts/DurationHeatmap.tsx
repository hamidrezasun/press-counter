import React, { useMemo } from 'react';
import { WorkSession } from '../../types';
import ReactECharts from 'echarts-for-react';

interface DurationHeatmapProps {
    sessions: WorkSession[];
    selectedDie: string;
}

const DurationHeatmap: React.FC<DurationHeatmapProps> = ({ sessions, selectedDie }) => {

    const { globalMaxDuration } = useMemo(() => {
        if (!sessions || sessions.length === 0) {
            return { globalMaxDuration: 0 };
        }
        
        const globalGrid: number[][] = Array(7).fill(0).map(() => Array(24).fill(0));
        sessions.forEach(session => {
             if (session.duration_seconds) {
                const startTime = new Date(session.start_time);
                const dayOfWeek = (startTime.getDay() + 1) % 7; // Saturday (6) -> 0, Sunday (0) -> 1, etc.
                const hour = startTime.getHours();
                globalGrid[dayOfWeek][hour] += session.duration_seconds;
            }
        });
        
        let gMax = 0;
        for (let day = 0; day < 7; day++) {
            for (let hour = 0; hour < 24; hour++) {
                if (globalGrid[day][hour] > gMax) {
                    gMax = globalGrid[day][hour];
                }
            }
        }
        return { globalMaxDuration: gMax };
    }, [sessions]);

    const heatmapData = useMemo(() => {
        if (!sessions || sessions.length === 0) {
            return [];
        }

        const filteredSessions = selectedDie === 'all'
            ? sessions
            : sessions.filter(s => (s.die_id || 'N/A') === selectedDie);
        
        const viewGrid: { duration: number; dies: Set<string> }[][] = Array(7).fill(0).map(() =>
            Array(24).fill(0).map(() => ({ duration: 0, dies: new Set() }))
        );

        filteredSessions.forEach(session => {
            if (session.duration_seconds) {
                const startTime = new Date(session.start_time);
                const dayOfWeek = (startTime.getDay() + 1) % 7; // Saturday (6) -> 0, Sunday (0) -> 1, etc.
                const hour = startTime.getHours();
                
                viewGrid[dayOfWeek][hour].duration += session.duration_seconds;
                if (session.die_id) {
                    viewGrid[dayOfWeek][hour].dies.add(session.die_id);
                }
            }
        });

        const data = [];
        for (let day = 0; day < 7; day++) {
            for (let hour = 0; hour < 24; hour++) {
                if (viewGrid[day][hour].duration > 0) {
                    data.push([
                        hour,
                        day,
                        viewGrid[day][hour].duration
                    ]);
                }
            }
        }
        return data;
    }, [sessions, selectedDie]);
    
    const days = ['شنبه', 'یک‌شنبه', 'دوشنبه', 'سه‌شنبه', 'چهارشنبه', 'پنج‌شنبه', 'جمعه'];
    const hours = Array.from({ length: 24 }, (_, i) => `${i.toString().padStart(2, '0')}:00`);

    const formatTooltipDuration = (seconds: number) => {
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        return `${h} ساعت و ${m} دقیقه`;
    };

    const getOption = () => ({
        backgroundColor: 'transparent',
        tooltip: {
            position: 'top',
            backgroundColor: 'rgba(31, 41, 55, 0.8)',
            borderColor: '#4b5563',
            textStyle: { color: '#ffffff' },
            formatter: (params: any) => {
                return `<strong>${days[params.value[1]]} ساعت ${hours[params.value[0]]}</strong><br/>مدت زمان: ${formatTooltipDuration(params.value[2])}`;
            }
        },
        grid: {
            height: '80%',
            top: '10%',
            left: '8%',
            right: '5%'
        },
        xAxis: {
            type: 'category',
            data: hours,
            splitArea: { show: true, areaStyle: { color: ['rgba(255,255,255,0.05)', 'rgba(0,0,0,0)'] } },
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0', interval: 2 }
        },
        yAxis: {
            type: 'category',
            data: days,
            splitArea: { show: false },
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0' }
        },
        visualMap: {
            min: 0,
            max: globalMaxDuration > 0 ? globalMaxDuration : 1,
            calculable: true,
            orient: 'horizontal',
            left: 'center',
            bottom: '0%',
            inRange: {
                color: ['#1f2937', '#5b21b6', '#c084fc'] // from gray-800 to purple
            },
            textStyle: { color: '#A0AEC0' },
            formatter: (value: number) => `${Math.floor(value / 3600)} ساعت`
        },
        series: [{
            name: 'مدت زمان',
            type: 'heatmap',
            data: heatmapData,
            label: { show: false },
            emphasis: {
                itemStyle: {
                    shadowBlur: 10,
                    shadowColor: 'rgba(255, 255, 255, 0.5)'
                }
            }
        }]
    });
    
    if (!sessions || sessions.length === 0) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">داده‌ای برای نقشه حرارتی موجود نیست.</p></div>;
    }

    return (
        <div className="h-full w-full flex flex-col">
            <ReactECharts
                option={getOption()}
                style={{ height: '100%', width: '100%' }}
                notMerge={true}
                lazyUpdate={true}
            />
        </div>
    );
};

export default DurationHeatmap;