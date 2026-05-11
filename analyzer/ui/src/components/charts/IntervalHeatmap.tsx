import React, { useMemo } from 'react';
import { WorkSession } from '../../types';
import ReactECharts from 'echarts-for-react';

interface IntervalHeatmapProps {
    sessions: WorkSession[];
    selectedDie: string;
}

const IntervalHeatmap: React.FC<IntervalHeatmapProps> = ({ sessions, selectedDie }) => {

    const { globalMaxInterval } = useMemo(() => {
        const validSessions = sessions.filter(s => s.average_press_interval_seconds > 0);
        if (validSessions.length === 0) {
            return { globalMaxInterval: 0 };
        }
        
        const globalGrid: { totalInterval: number; count: number }[][] = Array(7).fill(0).map(() =>
            Array(24).fill(0).map(() => ({ totalInterval: 0, count: 0 }))
        );

        validSessions.forEach(session => {
            const startTime = new Date(session.start_time);
            const dayOfWeek = (startTime.getDay() + 1) % 7; // Saturday (6) -> 0, Sunday (0) -> 1, etc.
            const hour = startTime.getHours();
            globalGrid[dayOfWeek][hour].totalInterval += session.average_press_interval_seconds;
            globalGrid[dayOfWeek][hour].count += 1;
        });

        let gMax = 0;
        for (let day = 0; day < 7; day++) {
            for (let hour = 0; hour < 24; hour++) {
                if (globalGrid[day][hour].count > 0) {
                    const avg = globalGrid[day][hour].totalInterval / globalGrid[day][hour].count;
                    if (avg > gMax) {
                        gMax = avg;
                    }
                }
            }
        }
        return { globalMaxInterval: gMax };
    }, [sessions]);

    const heatmapData = useMemo(() => {
        const validSessions = sessions.filter(s => s.average_press_interval_seconds > 0);
        if (validSessions.length === 0) {
            return [];
        }

        const filteredSessions = selectedDie === 'all'
            ? validSessions
            : validSessions.filter(s => (s.die_id || 'N/A') === selectedDie);
        
        const viewGrid: { totalInterval: number; count: number; dies: Set<string> }[][] = Array(7).fill(0).map(() =>
            Array(24).fill(0).map(() => ({ totalInterval: 0, count: 0, dies: new Set() }))
        );

        filteredSessions.forEach(session => {
            const startTime = new Date(session.start_time);
            const dayOfWeek = (startTime.getDay() + 1) % 7; // Saturday (6) -> 0, Sunday (0) -> 1, etc.
            const hour = startTime.getHours();
            
            viewGrid[dayOfWeek][hour].totalInterval += session.average_press_interval_seconds;
            viewGrid[dayOfWeek][hour].count += 1;
            if (session.die_id) {
                viewGrid[dayOfWeek][hour].dies.add(session.die_id);
            }
        });

        const data = [];
        for (let day = 0; day < 7; day++) {
            for (let hour = 0; hour < 24; hour++) {
                if (viewGrid[day][hour].count > 0) {
                    const avg = viewGrid[day][hour].totalInterval / viewGrid[day][hour].count;
                    data.push([
                        hour,
                        day,
                        avg
                    ]);
                }
            }
        }
        return data;
    }, [sessions, selectedDie]);
    
    const days = ['شنبه', 'یک‌شنبه', 'دوشنبه', 'سه‌شنبه', 'چهارشنبه', 'پنج‌شنبه', 'جمعه'];
    const hours = Array.from({ length: 24 }, (_, i) => `${i.toString().padStart(2, '0')}:00`);


    const getOption = () => ({
        backgroundColor: 'transparent',
        tooltip: {
            position: 'top',
            backgroundColor: 'rgba(31, 41, 55, 0.8)',
            borderColor: '#4b5563',
            textStyle: { color: '#ffffff' },
            formatter: (params: any) => {
                return `<strong>${days[params.value[1]]} ساعت ${hours[params.value[0]]}</strong><br/>میانگین فاصله: ${params.value[2].toFixed(2)} ثانیه`;
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
            max: globalMaxInterval > 0 ? globalMaxInterval : 1,
            calculable: true,
            orient: 'horizontal',
            left: 'center',
            bottom: '0%',
            inRange: {
                color: ['#2dd4bf', '#facc15', '#ef4444'] // teal (fast) -> yellow -> red (slow)
            },
            textStyle: { color: '#A0AEC0' },
            formatter: (value: number) => `${value.toFixed(1)}s`
        },
        series: [{
            name: 'میانگین فاصله',
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

export default IntervalHeatmap;