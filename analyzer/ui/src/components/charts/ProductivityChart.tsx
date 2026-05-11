import React from 'react';
import ReactECharts from 'echarts-for-react';

interface ProductivityChartProps {
    title: string;
    data: any[];
    dataKey: string;
    xAxisKey: string;
    barColor: string;
}

const ProductivityChart: React.FC<ProductivityChartProps> = ({ title, data, dataKey, xAxisKey, barColor }) => {
    
    const getOption = () => ({
        backgroundColor: 'transparent',
        title: {
            text: title,
            left: 'center',
            textStyle: {
                color: '#E2E8F0',
                fontSize: 16,
                fontWeight: 'normal'
            }
        },
        tooltip: {
            trigger: 'axis',
            axisPointer: { type: 'shadow' },
            backgroundColor: 'rgba(31, 41, 55, 0.8)',
            borderColor: '#4b5563',
            textStyle: { color: '#ffffff' },
            formatter: (params: any[]) => `${params[0].name}<br/>${params[0].marker} ${params[0].seriesName}: <strong>${params[0].value.toLocaleString()}</strong>`
        },
        grid: { top: '20%', left: '3%', right: '4%', bottom: '3%', containLabel: true },
        xAxis: {
            type: 'category',
            data: data.map(d => d[xAxisKey]),
            axisLine: { lineStyle: { color: '#4A5568' } },
            axisTick: { show: false },
            axisLabel: { color: '#A0AEC0' }
        },
        yAxis: {
            type: 'value',
            axisLine: { show: true, lineStyle: { color: '#4A5568' } },
            axisLabel: { color: '#A0AEC0' },
            splitLine: { lineStyle: { color: '#4A5568', type: 'dashed' } }
        },
        series: [
            {
                name: dataKey,
                type: 'bar',
                data: data.map(d => d[dataKey]),
                itemStyle: {
                    color: barColor,
                    borderRadius: [4, 4, 0, 0]
                },
                barWidth: '60%'
            }
        ]
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

export default ProductivityChart;