import React, { useState } from 'react';
import Card from './Card';
import Modal from './Modal';

interface ChartContainerProps {
  title: string;
  children: React.ReactNode;
  className?: string;
  headerControls?: React.ReactNode;
}

const ChartContainer: React.FC<ChartContainerProps> = ({ title, children, className, headerControls }) => {
    const [isFullScreen, setIsFullScreen] = useState(false);

    return (
        <>
            <Card className={`h-full flex flex-col ${className}`}>
                <div className="flex justify-between items-center mb-4">
                    <h3 className="text-lg font-semibold text-white">{title}</h3>
                    <div className="flex items-center gap-2">
                        {headerControls}
                        <button
                            onClick={() => setIsFullScreen(true)}
                            className="p-2 text-gray-400 hover:text-white transition-colors"
                            title="تمام صفحه"
                        >
                            <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5v-4m0 0h-4m4 0l-5-5" />
                            </svg>
                        </button>
                    </div>
                </div>
                <div className="flex-grow h-full w-full">
                    {children}
                </div>
            </Card>

            {isFullScreen && (
                <Modal onClose={() => setIsFullScreen(false)} title={title} size="fullscreen">
                    <div className="w-full h-full">
                        {children}
                    </div>
                </Modal>
            )}
        </>
    );
};

export default ChartContainer;
