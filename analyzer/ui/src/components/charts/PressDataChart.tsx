import React from 'react';
import { PressCounterRecord } from '../../types';
import { formatTime, formatDateTime } from '../../services/formatters';
import ReactECharts from 'echarts-for-react';

interface PressDataChartProps {
  data: PressCounterRecord[];
}

const PressDataChart: React.FC<PressDataChartProps> = ({ data }) => {
    if (!data || data.length === 0) {
        return <div className="flex items-center justify-center h-full"><p className="text-center text-gray-500 p-8">داده‌ای برای این جلسه موجود نیست.</p></div>;
    }

    // --- Anomaly Detection ---
    const temperatures = data.map(d => d.temperature);
    const durations = data.map(d => d.duration);
    
    const avgTemp = temperatures.length > 0 ? temperatures.reduce((a, b) => a + b, 0) / temperatures.length : 0;
    const stdDevTemp = temperatures.length > 0 ? Math.sqrt(temperatures.map(x => Math.pow(x - avgTemp, 2)).reduce((a, b) => a + b, 0) / temperatures.length) : 0;
    const upperTempThreshold = avgTemp + 2 * stdDevTemp;
    
    const avgDuration = durations.length > 0 ? durations.reduce((a, b) => a + b, 0) / durations.length : 0;
    const stdDevDuration = durations.length > 0 ? Math.sqrt(durations.map(x => Math.pow(x - avgDuration, 2)).reduce((a, b) => a + b, 0) / durations.length) : 0;
    const upperDurationThreshold = avgDuration + 2 * stdDevDuration;
    
    const tempAnomalies = data
        .filter(d => d.temperature > upperTempThreshold)
        .map(d => [d.timestamp, d.temperature]);

    const durationAnomalies = data
        .filter(d => d.duration > upperDurationThreshold)
        .map(d => [d.timestamp, d.duration]);

    const getOption = () => {
        return {
            backgroundColor: 'transparent',
            tooltip: {
                trigger: 'axis',
                backgroundColor: 'rgba(31, 41, 55, 0.8)',
                borderColor: '#4b5563',
                textStyle: { color: '#ffffff' },
                formatter: (params: any[]) => {
                    const timestamp = params[0].axisValue;
                    let tooltipText = `<strong>${formatDateTime(new Date(timestamp))}</strong><br/>`;
                    params.forEach(param => {
                        if (param.value === null || param.value === undefined) return;
                        const value = Array.isArray(param.value) ? param.value[1] : param.value;
                        let unit = '';
                        if (param.seriesName.includes('دما') || param.seriesName.includes('ناهنجاری دما')) unit = ' °C';
                        else if (param.seriesName.includes('زمان') || param.seriesName.includes('ناهنجاری زمان')) unit = ' ms';
                        tooltipText += `${param.marker}${param.seriesName}: <strong>${value.toFixed(param.seriesName.includes('دما') ? 1 : 0)}</strong>${unit}<br/>`;
                    });
                    return tooltipText;
                },
            },
            legend: {
                data: ['تعداد ضربه', 'دما (°C)', 'زمان (ms)', 'ناهنجاری دما', 'ناهنجاری زمان'],
                textStyle: { color: '#A0AEC0' },
                selected: {
                    'ناهنجاری دما': tempAnomalies.length > 0,
                    'ناهنجاری زمان': durationAnomalies.length > 0,
                }
            },
            grid: { left: '3%', right: '4%', bottom: '3%', containLabel: true },
            xAxis: {
                type: 'time',
                boundaryGap: false,
                axisLine: { lineStyle: { color: '#4A5568' } },
                axisLabel: { 
                    color: '#A0AEC0',
                    formatter: (value: number) => formatTime(new Date(value))
                }
            },
            yAxis: [
                {
                    type: 'value',
                    name: 'تعداد / زمان',
                    nameTextStyle: { color: '#A0AEC0' },
                    axisLine: { lineStyle: { color: '#4A5568' } },
                    axisLabel: { color: '#A0AEC0' },
                    splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } }
                },
                {
                    type: 'value',
                    name: 'دما (°C)',
                    nameTextStyle: { color: '#A0AEC0' },
                    axisLine: { lineStyle: { color: '#4A5568' } },
                    axisLabel: { color: '#A0AEC0' },
                    splitLine: { show: false }
                }
            ],
            series: [
                { name: 'تعداد ضربه', type: 'line', yAxisIndex: 0, data: data.map(d => [d.timestamp, d.count]), smooth: true, color: '#2DD4BF', showSymbol: false },
                { name: 'دما (°C)', type: 'line', yAxisIndex: 1, data: data.map(d => [d.timestamp, d.temperature]), smooth: true, color: '#FBBF24', showSymbol: false },
                { name: 'زمان (ms)', type: 'line', yAxisIndex: 0, data: data.map(d => [d.timestamp, d.duration]), smooth: true, color: '#A3E635', showSymbol: false },
                {
                    name: 'ناهنجاری دما',
                    type: 'scatter',
                    yAxisIndex: 1,
                    data: tempAnomalies,
                    symbolSize: 8,
                    itemStyle: { color: '#ef4444' }, // Red
                    zlevel: 10,
                },
                {
                    name: 'ناهنجاری زمان',
                    type: 'scatter',
                    yAxisIndex: 0,
                    data: durationAnomalies,
                    symbolSize: 8,
                    itemStyle: { color: '#f97316' }, // Orange
                    zlevel: 10,
                }
            ]
        };
    };

    return (
        <ReactECharts
            option={getOption()}
            style={{ height: '100%', width: '100%' }}
            notMerge={true}
            lazyUpdate={true}
        />
    );
};

export default PressDataChart;