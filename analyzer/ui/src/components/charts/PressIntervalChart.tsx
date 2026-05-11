import React, { useState } from 'react';
import { PressCounterRecord } from '../../types';
import ReactECharts from 'echarts-for-react';
import { formatTime, formatDateTime } from '../../services/formatters';
import IntervalDetailModal from './IntervalDetailModal';

interface PressIntervalChartProps {
  data: PressCounterRecord[];
  averageInterval: number;
}

const PressIntervalChart: React.FC<PressIntervalChartProps> = ({ data, averageInterval }) => {
    const [modalData, setModalData] = useState<{ records: PressCounterRecord[], highlightIndex: number } | null>(null);

    if (!data || data.length < 2) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">داده کافی برای نمایش فواصل وجود ندارد.</p></div>;
    }

    const intervalData = data.slice(1).map((record, index) => {
        const prevRecord = data[index];
        const interval = (new Date(record.timestamp).getTime() - new Date(prevRecord.timestamp).getTime()) / 1000;
        return {
            timestamp: record.timestamp,
            interval: interval
        };
    });

    const intervals = intervalData.map(d => d.interval);
    const stdDev = Math.sqrt(intervals.reduce((acc, val) => acc + Math.pow(val - averageInterval, 2), 0) / intervals.length);

    const upperAnomalyThreshold = averageInterval + 2 * stdDev;
    const lowerAnomalyThreshold = Math.max(0, averageInterval - 2 * stdDev);

    const anomalyData = intervalData
        .filter(d => d.interval > upperAnomalyThreshold || d.interval < lowerAnomalyThreshold)
        .map(d => [d.timestamp, d.interval]);
    
    const upperWarningThreshold = averageInterval + stdDev;
    const lowerWarningThreshold = Math.max(0, averageInterval - stdDev);

    const onChartClick = (params: any) => {
        if (params.seriesType !== 'line' && params.seriesType !== 'scatter') return;
        
        // params.dataIndex is the index in the *plotted* data (intervalData)
        // Find the timestamp from the clicked point
        const clickedTimestamp = Array.isArray(params.value) ? params.value[0] : params.name;

        // Find the corresponding record in the original data array
        const originalDataIndex = data.findIndex(d => d.timestamp === clickedTimestamp);

        if (originalDataIndex === -1) return; // Should not happen

        const startIndex = Math.max(0, originalDataIndex - 10);
        const endIndex = Math.min(data.length, originalDataIndex + 11);
        
        const slicedRecords = data.slice(startIndex, endIndex);
        const highlightIndexInSlice = originalDataIndex - startIndex;
        
        setModalData({ records: slicedRecords, highlightIndex: highlightIndexInSlice });
    };

    const getOption = () => ({
        backgroundColor: 'transparent',
        title: {
            text: 'تحلیل فواصل ضربات (سری زمانی)',
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
                const dataPoint = params.find(p => p.seriesName === 'فاصله (ثانیه)') || params[0];
                const timestamp = dataPoint.axisValue;
                const value = Array.isArray(dataPoint.value) ? dataPoint.value[1] : dataPoint.value;
                if (value === null || value === undefined) return '';

                let tooltipText = `زمان: <strong>${formatDateTime(new Date(timestamp))}</strong><br/>`;
                
                params.forEach(param => {
                    const seriesValue = Array.isArray(param.value) ? param.value[1] : param.value;
                     if (seriesValue !== null && seriesValue !== undefined) {
                        tooltipText += `${param.marker}${param.seriesName}: <strong>${seriesValue.toFixed(2)} s</strong><br/>`;
                     }
                });

                return tooltipText;
            }
        },
        grid: { top: '20%', left: '3%', right: '4%', bottom: '3%', containLabel: true },
        xAxis: {
            type: 'time',
            name: 'زمان',
            nameLocation: 'middle',
            nameGap: 30,
            nameTextStyle: { color: '#A0AEC0' },
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisLabel: { 
                color: '#A0AEC0',
                formatter: (value: number) => formatTime(new Date(value))
            }
        },
        yAxis: {
            type: 'value',
            name: 'فاصله (ثانیه)',
            nameTextStyle: { color: '#A0AEC0' },
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0' },
            splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } }
        },
        legend: {
            data: ['فاصله (ثانیه)', 'ناهنجاری'],
            textStyle: { color: '#A0AEC0' },
            top: 30,
            right: 10
        },
        series: [
            {
                name: 'فاصله (ثانیه)',
                type: 'line',
                data: intervalData.map(d => [d.timestamp, d.interval]),
                smooth: true,
                color: '#f472b6',
                showSymbol: false,
                cursor: 'pointer',
                markLine: {
                    silent: true,
                    data: [{
                        yAxis: averageInterval,
                        name: 'میانگین',
                        label: {
                            formatter: `میانگین (${averageInterval.toFixed(2)}s)`,
                            position: 'insideEndTop',
                            color: '#fbbf24'
                        },
                        lineStyle: {
                            color: '#fbbf24',
                            type: 'dashed'
                        }
                    }]
                },
                markArea: {
                    silent: true,
                    itemStyle: { color: 'rgba(74, 222, 128, 0.1)' },
                    data: [[{ yAxis: lowerWarningThreshold }, { yAxis: upperWarningThreshold }]],
                    label: {
                        formatter: 'بازه نرمال (1σ)',
                        position: 'insideTopLeft',
                        color: 'rgba(74, 222, 128, 0.7)',
                        fontSize: 10
                    }
                }
            },
            {
                name: 'ناهنجاری',
                type: 'scatter',
                data: anomalyData,
                symbolSize: 8,
                cursor: 'pointer',
                itemStyle: {
                    color: '#ef4444', // Red for anomalies
                    borderColor: '#fca5a5',
                    borderWidth: 1,
                },
                zlevel: 10
            }
        ]
    });

    return (
        <>
            <ReactECharts
                option={getOption()}
                style={{ height: '100%', width: '100%' }}
                notMerge={true}
                lazyUpdate={true}
                onEvents={{ 'click': onChartClick }}
            />
            {modalData && (
                <IntervalDetailModal
                    records={modalData.records}
                    highlightIndex={modalData.highlightIndex}
                    onClose={() => setModalData(null)}
                />
            )}
        </>
    );
};

export default PressIntervalChart;