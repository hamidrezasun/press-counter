import React from 'react';
import { PressCounterRecord } from '../../types';
import Modal from '../ui/Modal';
import { formatDateTime } from '../../services/formatters';

interface IntervalDetailModalProps {
  records: PressCounterRecord[];
  highlightIndex: number;
  onClose: () => void;
}

const IntervalDetailModal: React.FC<IntervalDetailModalProps> = ({ records, highlightIndex, onClose }) => {
  return (
    <Modal onClose={onClose} title="جزئیات زمینه رکورد ضربه">
      <div className="overflow-x-auto">
        <p className="text-sm text-gray-400 mb-4">
          نمایش رکوردهای اطراف رویداد انتخاب شده. سطر هایلایت شده نشان‌دهنده نقطه داده کلیک شده است.
        </p>
        <table className="min-w-full text-sm text-left text-gray-300">
          <thead className="text-xs text-teal-300 uppercase bg-gray-700/50">
            <tr>
              <th scope="col" className="px-4 py-3">برچسب زمانی</th>
              <th scope="col" className="px-4 py-3 text-right">تعداد ضربه</th>
              <th scope="col" className="px-4 py-3 text-right">زمان (میلی‌ثانیه)</th>
              <th scope="col" className="px-4 py-3 text-right">دما (°C)</th>
              <th scope="col" className="px-4 py-3 text-right">فاصله (ثانیه)</th>
            </tr>
          </thead>
          <tbody>
            {records.map((record, index) => {
              const isHighlighted = index === highlightIndex;
              let interval: number | null = null;
              if (index > 0) {
                const prevRecord = records[index - 1];
                interval = (new Date(record.timestamp).getTime() - new Date(prevRecord.timestamp).getTime()) / 1000;
              }

              return (
                <tr
                  key={record.timestamp}
                  className={`border-b border-gray-700 font-mono ${isHighlighted ? 'bg-teal-500/20' : 'hover:bg-gray-700/50'}`}
                >
                  <td className={`px-4 py-2 font-semibold ${isHighlighted ? 'text-teal-300' : 'text-white'}`}>
                    {formatDateTime(record.timestamp)}
                  </td>
                  <td className="px-4 py-2 text-right">{record.count}</td>
                  <td className="px-4 py-2 text-right">{record.duration}</td>
                  <td className="px-4 py-2 text-right">{record.temperature.toFixed(1)}</td>
                  <td className="px-4 py-2 text-right">
                    {interval !== null ? interval.toFixed(2) : 'N/A'}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </Modal>
  );
};

export default IntervalDetailModal;