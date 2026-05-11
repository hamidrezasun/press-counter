import React, { useMemo, useState, useCallback } from 'react';
import { PressCounterRecord, WorkSession } from '../../types';
import ReactECharts from 'echarts-for-react';
import { formatDateTime, formatDate } from '../../services/formatters';
import IntervalDetailModal from './IntervalDetailModal';

interface AllPressIntervalsChartProps {
  pressData: PressCounterRecord[];
  sessions: WorkSession[];
  filterRange: { start: string; end: string };
}

const AllPressIntervalsChart: React.FC<AllPressIntervalsChartProps> = ({ pressData, sessions, filterRange }) => {
    const [modalData, setModalData] = useState<{ records: PressCounterRecord[], highlightIndex: number } | null>(null);

    const chartData = useMemo(() => {
        const { start, end } = filterRange;
        const startTime = start ? new Date(start).getTime() : -Infinity;
        const endTime = end ? new Date(end).getTime() : Infinity;

        const dataToProcess = pressData.filter(p => {
            const pTime = new Date(p.timestamp).getTime();
            return pTime >= startTime && pTime <= endTime;
        });

        if (dataToProcess.length < 2) {
            return null;
        }

        // 1. Calculate intervals between all presses
        const intervalData = [];
        for (let i = 1; i < dataToProcess.length; i++) {
            const prevRecord = dataToProcess[i - 1];
            const record = dataToProcess[i];
            const interval = (new Date(record.timestamp).getTime() - new Date(prevRecord.timestamp).getTime()) / 1000;
            if (interval >= 0) { // Ensure no negative intervals due to weird data
                intervalData.push({
                    timestamp: record.timestamp,
                    interval: interval
                });
            }
        }
        
        if (intervalData.length === 0) return null;

        // 2. Calculate stats on "normal" intervals (not gaps between sessions)
        const normalIntervals = intervalData.filter(d => d.interval < 300).map(d => d.interval); // 5 min threshold
        const sum = normalIntervals.reduce((a, b) => a + b, 0);
        const mean = normalIntervals.length > 0 ? sum / normalIntervals.length : 0;
        const stdDev = normalIntervals.length > 0 ? Math.sqrt(normalIntervals.reduce((acc, val) => acc + Math.pow(val - mean, 2), 0) / normalIntervals.length) : 0;

        // 3. Identify anomalies
        const upperAnomalyThreshold = mean + 3 * stdDev; // Use 3x stdDev for global view to reduce noise
        const anomalyData = intervalData
            .filter(d => d.interval > 0 && d.interval > upperAnomalyThreshold)
            .map(d => [d.timestamp, d.interval]);

        // 4. Identify idle time between sessions that falls within the view
        const sortedSessions = [...sessions].sort((a, b) => new Date(a.start_time).getTime() - new Date(b.start_time).getTime());
        const idleTimeAreas = [];
        for (let i = 0; i < sortedSessions.length - 1; i++) {
            const sessionA = sortedSessions[i];
            const sessionB = sortedSessions[i+1];
            if (sessionA.end_time) {
                 const idleStart = new Date(sessionA.end_time).getTime();
                 const idleEnd = new Date(sessionB.start_time).getTime();
                 if (idleStart < endTime && idleEnd > startTime) { // Overlap check
                    idleTimeAreas.push([
                        { name: 'زمان بیکاری', xAxis: sessionA.end_time },
                        { xAxis: sessionB.start_time }
                    ]);
                 }
            }
        }

        return {
            intervalData,
            anomalyData,
            idleTimeAreas,
            stats: { mean }
        };

    }, [pressData, sessions, filterRange]);

    const onChartClick = useCallback((params: any) => {
        if (params.seriesType !== 'line' && params.seriesType !== 'scatter') return;
        
        const clickedTimestamp = Array.isArray(params.value) ? params.value[0] : params.name;

        const originalDataIndex = pressData.findIndex(d => d.timestamp === clickedTimestamp);

        if (originalDataIndex === -1) return;

        const startIndex = Math.max(0, originalDataIndex - 10);
        const endIndex = Math.min(pressData.length, originalDataIndex + 11);
        
        const slicedRecords = pressData.slice(startIndex, endIndex);
        const highlightIndexInSlice = originalDataIndex - startIndex;
        
        setModalData({ records: slicedRecords, highlightIndex: highlightIndexInSlice });
    }, [pressData]);
    
    const getOption = () => {
        const { start, end } = filterRange;
        const subtext = start && end
            ? `نمایش از ${formatDateTime(start)} تا ${formatDateTime(end)} | میانگین فاصله: ${chartData?.stats.mean.toFixed(2)}s`
            : `نواحی خاکستری زمان بیکاری است. میانگین فاصله عملیاتی: ${chartData?.stats.mean.toFixed(2)}s`;
        
        const option: any = {
            backgroundColor: 'transparent',
            title: {
                text: 'سری زمانی کلی فواصل ضربات',
                subtext: subtext,
                left: 'center',
                textStyle: { color: '#E2E8F0', fontSize: 16 },
                subtextStyle: { color: '#A0AEC0' }
            },
            tooltip: {
                trigger: 'axis',
                backgroundColor: 'rgba(31, 41, 55, 0.8)',
                borderColor: '#4b5563',
                textStyle: { color: '#ffffff' },
                formatter: (params: any[]) => {
                    const dataPoint = params[0];
                    const timestamp = dataPoint.axisValue;
                    const value = Array.isArray(dataPoint.value) ? dataPoint.value[1] : dataPoint.value;
                    if (value === null || value === undefined) return '';

                    return `زمان: <strong>${formatDateTime(new Date(timestamp))}</strong><br/>فاصله: <strong>${value.toFixed(2)} s</strong>`;
                }
            },
            grid: { top: '25%', left: '3%', right: '4%', bottom: '3%', containLabel: true },
            xAxis: {
                type: 'time',
                min: start ? new Date(start).getTime() : null,
                max: end ? new Date(end).getTime() : null,
                axisLine: { lineStyle: { color: '#4A5568' } },
                axisLabel: { 
                    color: '#A0AEC0',
                    formatter: (value: number) => formatDate(new Date(value))
                }
            },
            yAxis: {
                type: 'log',
                logBase: 10,
                name: 'فاصله (ثانیه)',
                nameTextStyle: { color: '#A0AEC0' },
                axisLine: { lineStyle: { color: '#4A5568' } },
                axisLabel: { color: '#A0AEC0' },
                splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } }
            },
            legend: {
                data: ['فاصله', 'ناهنجاری'],
                textStyle: { color: '#A0AEC0' },
                top: 45,
                right: 10
            },
            series: [
                {
                    name: 'فاصله',
                    type: 'line',
                    data: chartData?.intervalData.map(d => [d.timestamp, d.interval]),
                    smooth: false,
                    color: '#2DD4BF',
                    showSymbol: false,
                    cursor: 'pointer',
                    markLine: {
                        silent: true,
                        data: [{
                            yAxis: chartData?.stats.mean,
                            name: 'میانگین',
                            label: {
                                formatter: `میانگین (${chartData?.stats.mean.toFixed(2)}s)`,
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
                        itemStyle: { color: 'rgba(107, 114, 128, 0.25)', opacity: 1 },
                        data: chartData?.idleTimeAreas,
                        label: {
                            show: true,
                            position: 'insideTop',
                            color: '#9ca3af',
                            fontSize: 10,
                            formatter: 'بیکار',
                            distance: 10
                        }
                    }
                },
                {
                    name: 'ناهنجاری',
                    type: 'scatter',
                    data: chartData?.anomalyData,
                    symbolSize: 6,
                    cursor: 'pointer',
                    itemStyle: {
                        color: '#f472b6',
                    },
                    zlevel: 10
                }
            ]
        };

        return option;
    };

    if (!chartData) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">داده‌ای برای این بازه زمانی یافت نشد.</p></div>;
    }


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

export default AllPressIntervalsChart;