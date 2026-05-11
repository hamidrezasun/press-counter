import React from 'react';
import { DeviceInfo } from '../../types';
// FIX: Corrected import path for date formatting functions.
import { formatDate, formatDateTime } from '../../services/formatters';
import ReactECharts from 'echarts-for-react';

const SignalStrengthChart: React.FC<{ data: DeviceInfo[] }> = ({ data }) => {
    if (!data || data.length === 0) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">No signal data available.</p></div>;
    }
    
    const formattedData = data
        .sort((a,b) => new Date(a.timestamp).getTime() - new Date(b.timestamp).getTime())
        .map(d => ({
            time: d.timestamp,
            rssi: d.rssi
        }));

    const getOption = () => ({
        backgroundColor: 'transparent',
        title: {
            text: 'Signal Strength (RSSI) Over Time',
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
                return `Time: <strong>${formatDateTime(dataPoint.time)}</strong><br/>RSSI: <strong>${params[0].value} dBm</strong>`;
            }
        },
        grid: { top: '20%', left: '3%', right: '4%', bottom: '15%', containLabel: true },
        xAxis: {
            type: 'category',
            boundaryGap: false,
            data: formattedData.map(d => d.time),
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0', rotate: 30, formatter: (value: string) => formatDate(value) }
        },
        yAxis: {
            type: 'value',
            axisLine: { show: true, lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0', formatter: '{value} dBm' },
            splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } }
        },
        series: [{
            name: 'RSSI',
            type: 'line',
            data: formattedData.map(d => d.rssi),
            smooth: true,
            showSymbol: false,
            color: '#8b5cf6',
            areaStyle: {
                color: {
                    type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                    colorStops: [{ offset: 0, color: 'rgba(139, 92, 246, 0.5)' }, { offset: 1, color: 'rgba(139, 92, 246, 0)' }]
                }
            }
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

export default SignalStrengthChart;