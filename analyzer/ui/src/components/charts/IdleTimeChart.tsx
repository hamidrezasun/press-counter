import React, { useMemo } from 'react';
import { WorkSession } from '../../types';
import ReactECharts from 'echarts-for-react';
import { formatDateTime, formatDate } from '../../services/formatters';

// Fix: Import echarts and necessary components for custom chart rendering to resolve "Cannot find name 'echarts'".
import * as echarts from 'echarts/core';
import { CustomChart } from 'echarts/charts';
import { TooltipComponent, GridComponent } from 'echarts/components';
import { CanvasRenderer } from 'echarts/renderers';

echarts.use([TooltipComponent, GridComponent, CustomChart, CanvasRenderer]);

interface IdleTimeChartProps {
    sessions: WorkSession[];
    startDate: string;
    endDate: string;
}

const formatDuration = (seconds: number | null): string => {
    if (seconds === null || seconds < 0) return 'N/A';
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = Math.floor(seconds % 60);
    return `${h} ساعت ${m} دقیقه ${s} ثانیه`;
};

const IdleTimeChart: React.FC<IdleTimeChartProps> = ({ sessions, startDate, endDate }) => {

    const chartData = useMemo(() => {
        if (!sessions || sessions.length === 0 || !startDate || !endDate) {
            return { data: [], categories: [] };
        }

        const rangeStart = new Date(startDate).getTime();
        const rangeEnd = new Date(endDate).getTime();

        const sortedSessions = [...sessions]
            .filter(s => s.end_time) // Only consider sessions that have ended
            .sort((a, b) => new Date(a.start_time).getTime() - new Date(b.start_time).getTime());
        
        const idlePeriods = [];

        for (let i = 0; i < sortedSessions.length - 1; i++) {
            const sessionA = sortedSessions[i];
            const sessionB = sortedSessions[i+1];

            const idleStart = new Date(sessionA.end_time!).getTime();
            const idleEnd = new Date(sessionB.start_time).getTime();
            
            // Check if the idle period overlaps with the selected range
            if (idleStart < rangeEnd && idleEnd > rangeStart) {
                const duration = (idleEnd - idleStart) / 1000;
                 if (duration > 0) {
                    idlePeriods.push({
                        start: idleStart,
                        end: idleEnd,
                        duration,
                        name: `بیکاری: ${formatDuration(duration)}`
                    });
                 }
            }
        }
        
        const formattedData = idlePeriods.map((item, index) => {
             // value: [categoryIndex, start, end, duration]
            return {
                name: item.name,
                value: [0, item.start, item.end, item.duration]
            };
        });

        return { data: formattedData, categories: ['دوره‌های بیکاری'] };

    }, [sessions, startDate, endDate]);

     const renderItem = (params: any, api: any) => {
        const categoryIndex = api.value(0);
        const start = api.coord([api.value(1), categoryIndex]);
        const end = api.coord([api.value(2), categoryIndex]);
        const height = api.size([0, 1])[1] * 0.6;

        const rectShape = echarts.graphic.clipRectByRect(
            {
                x: start[0],
                y: start[1] - height / 2,
                width: end[0] - start[0],
                height: height
            },
            {
                x: params.coordSys.x,
                y: params.coordSys.y,
                width: params.coordSys.width,
                height: params.coordSys.height
            }
        );

        return (
            rectShape && {
                type: 'rect',
                transition: ['shape'],
                shape: rectShape,
                style: api.style()
            }
        );
    };


    const getOption = () => ({
        backgroundColor: 'transparent',
        tooltip: {
            backgroundColor: 'rgba(31, 41, 55, 0.8)',
            borderColor: '#4b5563',
            textStyle: { color: '#ffffff' },
            formatter: (params: any) => {
                const value = params.value;
                return `
                    <strong>دوره بیکاری</strong><br/>
                    شروع: ${formatDateTime(new Date(value[1]))}<br/>
                    پایان: ${formatDateTime(new Date(value[2]))}<br/>
                    مدت: ${formatDuration(value[3])}
                `;
            }
        },
        grid: {
            left: '3%',
            right: '4%',
            bottom: '3%',
            containLabel: true
        },
        xAxis: {
            type: 'time',
            min: new Date(startDate).getTime(),
            max: new Date(endDate).getTime(),
            axisLabel: { 
                color: '#A0AEC0',
                formatter: (value: number) => formatDate(new Date(value))
            },
            axisLine: { lineStyle: { color: '#4A5568' } },
        },
        yAxis: {
            type: 'category',
            data: chartData.categories,
            axisLabel: { color: '#A0AEC0' },
            axisLine: { lineStyle: { color: '#4A5568' } },
        },
        series: [{
            type: 'custom',
            renderItem: renderItem,
            itemStyle: {
                color: '#38bdf8' // light blue
            },
            encode: {
                x: [1, 2], // map start and end time to x-axis
                y: 0      // map category index to y-axis
            },
            data: chartData.data
        }]
    });

    if (chartData.data.length === 0) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">هیچ دوره بیکاری در این بازه زمانی یافت نشد.</p></div>;
    }

    return (
        <div className="h-full w-full -ms-4">
            <ReactECharts
                // Fix: Pass the modularly imported echarts instance to the component.
                echarts={echarts}
                option={getOption()}
                style={{ height: '100%', width: '100%' }}
                notMerge={true}
                lazyUpdate={true}
            />
        </div>
    );
};

export default IdleTimeChart;