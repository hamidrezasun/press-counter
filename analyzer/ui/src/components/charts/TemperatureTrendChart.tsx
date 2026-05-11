import React from 'react';
import { WorkSession } from '../../types';
// FIX: Corrected import path for date formatting functions.
import { formatDate, formatDateTime } from '../../services/formatters';
import ReactECharts from 'echarts-for-react';

const TemperatureTrendChart: React.FC<{ data: WorkSession[] }> = ({ data }) => {
    if (!data || data.length === 0) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">No session data for temperature trends.</p></div>;
    }

    const formattedData = data
        .filter(s => s.end_time && s.total_count > 0) // Only show completed sessions with activity
        .sort((a,b) => new Date(a.start_time).getTime() - new Date(b.start_time).getTime())
        .map(d => ({
            time: d.start_time,
            avg_temp: d.average_temperature
        }));

    const getOption = () => ({
        backgroundColor: 'transparent',
        title: {
            text: 'Average Session Temperature',
            left: 'center',
            textStyle: {
                color: '#E2E8F0',
                fontSize: 16,
                fontWeight: 'normal'
            }
        },
        tooltip: {
            trigger: 'axis',
            backgroundColor: 'rgba(31, 41, 55, 0.8)',
            borderColor: '#4b5563',
            textStyle: { color: '#ffffff' },
            formatter: (params: any[]) => {
                const dataPoint = formattedData[params[0].dataIndex];
                return `Session Start: <strong>${formatDateTime(dataPoint.time)}</strong><br/>${params[0].marker} ${params[0].seriesName}: <strong>${params[0].value.toFixed(1)}</strong>`;
            }
        },
        legend: {
            data: ['Avg Temp (°C)'],
            textStyle: { color: '#A0AEC0' },
            top: 30
        },
        grid: { top: '20%', left: '3%', right: '4%', bottom: '15%', containLabel: true },
        xAxis: {
            type: 'category',
            data: formattedData.map(d => d.time),
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0', rotate: 30, formatter: (value: string) => formatDate(value) }
        },
        yAxis: {
            type: 'value',
            axisLine: { show: true, lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0', formatter: '{value} °C' },
            splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } }
        },
        series: [{
            name: 'Avg Temp (°C)',
            type: 'line',
            data: formattedData.map(d => d.avg_temp),
            smooth: true,
            color: '#FBBF24',
        }]
    });

    return (
        <div className="flex-grow h-full w-full">
             <ReactECharts
                option={getOption()}
                style={{ height: '100%', width: '100%' }}
                notMerge={true}
                lazyUpdate={true}
            />
        </div>
    );
};

export default TemperatureTrendChart;