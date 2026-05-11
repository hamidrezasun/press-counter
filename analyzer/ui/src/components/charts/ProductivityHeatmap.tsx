import React, { useMemo } from 'react';
import { WorkSession } from '../../types';
import ReactECharts from 'echarts-for-react';

interface ProductivityHeatmapProps {
    sessions: WorkSession[];
    selectedDie: string;
}

const ProductivityHeatmap: React.FC<ProductivityHeatmapProps> = ({ sessions, selectedDie }) => {

    const { globalMaxPresses } = useMemo(() => {
        if (!sessions || sessions.length === 0) {
            return { globalMaxPresses: 0 };
        }
        
        const globalGrid: number[][] = Array(7).fill(0).map(() => Array(24).fill(0));
        sessions.forEach(session => {
            const startTime = new Date(session.start_time);
            // Saturday (6) -> 0, Sunday (0) -> 1, etc.
            const dayOfWeek = (startTime.getDay() + 1) % 7; 
            const hour = startTime.getHours();
            globalGrid[dayOfWeek][hour] += session.total_count;
        });
        
        let gMax = 0;
        for (let day = 0; day < 7; day++) {
            for (let hour = 0; hour < 24; hour++) {
                if (globalGrid[day][hour] > gMax) {
                    gMax = globalGrid[day][hour];
                }
            }
        }
        
        return { globalMaxPresses: gMax };
    }, [sessions]);

    const heatmapData = useMemo(() => {
        if (!sessions || sessions.length === 0) {
            return [];
        }

        const filteredSessions = selectedDie === 'all'
            ? sessions
            : sessions.filter(s => (s.die_id || 'N/A') === selectedDie);
        
        const viewGrid: { count: number; dies: Set<string> }[][] = Array(7).fill(0).map(() =>
            Array(24).fill(0).map(() => ({ count: 0, dies: new Set() }))
        );

        filteredSessions.forEach(session => {
            const startTime = new Date(session.start_time);
            const dayOfWeek = (startTime.getDay() + 1) % 7; // Saturday (6) -> 0, Sunday (0) -> 1, etc.
            const hour = startTime.getHours();
            
            viewGrid[dayOfWeek][hour].count += session.total_count;
            if (session.die_id) {
                viewGrid[dayOfWeek][hour].dies.add(session.die_id);
            }
        });

        const data = [];
        for (let day = 0; day < 7; day++) {
            for (let hour = 0; hour < 24; hour++) {
                if (viewGrid[day][hour].count > 0) {
                    data.push([
                        hour,
                        day,
                        viewGrid[day][hour].count
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
                return `<strong>${days[params.value[1]]} ساعت ${hours[params.value[0]]}</strong><br/>تعداد ضربات: ${params.value[2].toLocaleString()}`;
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
            axisLabel: {
                color: '#A0AEC0',
                interval: 2 // Show every 3rd label
            }
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
            max: globalMaxPresses > 0 ? globalMaxPresses : 1,
            calculable: true,
            orient: 'horizontal',
            left: 'center',
            bottom: '0%',
            inRange: {
                color: ['#1f2937', '#115e59', '#2dd4bf'] // from gray-800 to teal
            },
            textStyle: {
                color: '#A0AEC0'
            }
        },
        series: [{
            name: 'ضربات',
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

export default ProductivityHeatmap;