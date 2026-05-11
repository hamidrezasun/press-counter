import React from 'react';
import { WorkSession } from '../../types';
import ReactECharts from 'echarts-for-react';

// --- ECHARTS MODULAR IMPORT ---
// Core ECharts
import * as echarts from 'echarts/core';
// Import charts, features, and components
import {
  BoxplotChart,
  ScatterChart,
} from 'echarts/charts';
import {
  TitleComponent,
  TooltipComponent,
  GridComponent,
} from 'echarts/components';
import { CanvasRenderer } from 'echarts/renderers';
// Import the specific function needed from the dataTool extension
import { prepareBoxplotData } from 'echarts/extension/dataTool';

// Register the required components
echarts.use([
  TitleComponent,
  TooltipComponent,
  GridComponent,
  BoxplotChart,
  ScatterChart,
  CanvasRenderer,
]);
// --- END ECHARTS MODULAR IMPORT ---

interface PerformanceDistributionChartProps {
    sessions: WorkSession[];
}

const PerformanceDistributionChart: React.FC<PerformanceDistributionChartProps> = ({ sessions }) => {
    if (!sessions || sessions.length < 2) {
        return <div className="h-full flex items-center justify-center"><p className="text-gray-500">داده کافی برای تحلیل توزیع وجود ندارد.</p></div>;
    }

    const intervalData = sessions
        .filter(s => s.average_press_interval_seconds > 0)
        .map(s => s.average_press_interval_seconds);

    const durationData = sessions
        .filter(s => s.average_press_duration_ms > 0)
        .map(s => s.average_press_duration_ms);

    // Use the imported function directly
    const boxplotData = prepareBoxplotData([
        intervalData,
        durationData
    ]);

    const getOption = () => ({
        backgroundColor: 'transparent',
        title: {
            text: 'توزیع عملکرد جلسات',
            left: 'center',
            textStyle: { color: '#E2E8F0', fontSize: 16 }
        },
        tooltip: {
            trigger: 'item',
            axisPointer: { type: 'shadow' },
            backgroundColor: 'rgba(31, 41, 55, 0.8)',
            borderColor: '#4b5563',
            textStyle: { color: '#ffffff' },
        },
        grid: { top: '20%', left: '3%', right: '4%', bottom: '3%', containLabel: true },
        xAxis: {
            type: 'category',
            data: ['میانگین فاصله (ثانیه)', 'میانگین زمان (میلی‌ثانیه)'],
            boundaryGap: true,
            nameGap: 30,
            splitArea: { show: false },
            axisLabel: { color: '#A0AEC0' },
            axisTick: { show: false },
            axisLine: { lineStyle: { color: '#4A5568' } },
        },
        yAxis: {
            type: 'value',
            name: 'مقدار (مقیاس لگاریتمی)',
            nameTextStyle: { color: '#A0AEC0' },
            axisLine: { show: true, lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0' },
            splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } },
            logBase: 10 // Use log scale for better visualization of varied data
        },
        series: [
            {
                name: 'Boxplot',
                type: 'boxplot',
                data: boxplotData.boxData,
                itemStyle: {
                    color: '#2DD4BF', // Bar color
                    borderColor: '#A3E635' // Border color
                }
            },
            {
                name: 'Outliers',
                type: 'scatter',
                data: boxplotData.outliers,
                itemStyle: {
                    color: '#f472b6' // Outlier color
                }
            }
        ]
    });

    return (
        <ReactECharts
            echarts={echarts} // Pass the modularly imported echarts instance
            option={getOption()}
            style={{ height: '100%', width: '100%' }}
            notMerge={true}
            lazyUpdate={true}
        />
    );
};

export default PerformanceDistributionChart;