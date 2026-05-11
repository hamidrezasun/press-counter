import React from 'react';
import { WorkSession, DeviceInfo } from '../../types';
import { formatDateTime, formatDate } from '../../services/formatters';
import ReactECharts from 'echarts-for-react';

interface DeviceHealthChartProps {
    sessions: WorkSession[];
    deviceInfo: DeviceInfo[];
}

const DeviceHealthChart: React.FC<DeviceHealthChartProps> = ({ sessions, deviceInfo }) => {
    if ((!sessions || sessions.length === 0) && (!deviceInfo || deviceInfo.length === 0)) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">داده‌ای برای نمودار سلامت موجود نیست.</p></div>;
    }

    const tempData = sessions
        .filter(s => s.end_time && s.total_count > 0)
        .map(s => [s.start_time, s.average_temperature.toFixed(1)]);

    const rssiData = deviceInfo.map(d => [d.timestamp, d.rssi]);

    const getOption = () => ({
        backgroundColor: 'transparent',
        tooltip: {
            trigger: 'axis',
            backgroundColor: 'rgba(31, 41, 55, 0.8)',
            borderColor: '#4b5563',
            textStyle: { color: '#ffffff' },
            formatter: (params: any[]) => {
                let tooltip = `<strong>${formatDateTime(params[0].axisValue)}</strong>`;
                params.forEach(param => {
                    const unit = param.seriesName === 'دما' ? '°C' : 'dBm';
                    tooltip += `<br/>${param.marker} ${param.seriesName}: <strong>${param.value[1]} ${unit}</strong>`;
                });
                return tooltip;
            }
        },
        legend: {
            data: ['دما', 'RSSI'],
            textStyle: { color: '#A0AEC0' },
            top: 40
        },
        grid: { top: '25%', left: '3%', right: '4%', bottom: '3%', containLabel: true },
        xAxis: {
            type: 'time',
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisLabel: { 
                color: '#A0AEC0',
                formatter: (value: number) => formatDate(new Date(value))
            }
        },
        yAxis: [
            {
                type: 'value',
                name: 'دما (°C)',
                nameTextStyle: { color: '#FBBF24' },
                position: 'left',
                axisLine: { show: true, lineStyle: { color: '#FBBF24' } },
                axisLabel: { color: '#FBBF24' },
                splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } }
            },
            {
                type: 'value',
                name: 'RSSI (dBm)',
                nameTextStyle: { color: '#8b5cf6' },
                position: 'right',
                axisLine: { show: true, lineStyle: { color: '#8b5cf6' } },
                axisLabel: { color: '#8b5cf6' },
                splitLine: { show: false }
            }
        ],
        series: [
            {
                name: 'دما',
                type: 'line',
                yAxisIndex: 0,
                data: tempData,
                smooth: true,
                showSymbol: false,
                color: '#FBBF24'
            },
            {
                name: 'RSSI',
                type: 'line',
                yAxisIndex: 1,
                data: rssiData,
                smooth: true,
                showSymbol: false,
                color: '#8b5cf6',
                areaStyle: {
                    color: {
                        type: 'linear', x: 0, y: 0, x2: 0, y2: 1,
                        colorStops: [{ offset: 0, color: 'rgba(139, 92, 246, 0.5)' }, { offset: 1, color: 'rgba(139, 92, 246, 0)' }]
                    }
                }
            }
        ]
    });

    return (
        <ReactECharts
            option={getOption()}
            style={{ height: '100%', width: '100%' }}
            notMerge={true}
            lazyUpdate={true}
        />
    );
};

export default DeviceHealthChart;