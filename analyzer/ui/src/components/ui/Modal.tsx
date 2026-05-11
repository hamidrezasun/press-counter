import React, { useEffect } from 'react';

interface ModalProps {
  children: React.ReactNode;
  onClose: () => void;
  title: string;
  size?: 'default' | 'fullscreen';
}

const Modal: React.FC<ModalProps> = ({ children, onClose, title, size = 'default' }) => {
  useEffect(() => {
    const handleEsc = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        onClose();
      }
    };
    window.addEventListener('keydown', handleEsc);
    return () => {
      window.removeEventListener('keydown', handleEsc);
    };
  }, [onClose]);

  const sizeClasses = size === 'fullscreen'
    ? 'max-w-none w-[98vw] h-[98vh] max-h-[98vh]'
    : 'max-w-4xl max-h-[90vh]';

  const contentPadding = size === 'fullscreen' ? 'p-2' : 'p-6';

  return (
    <div
      className="fixed inset-0 bg-black/70 backdrop-blur-sm flex justify-center items-center z-50 p-4"
      onClick={onClose}
    >
      <div
        className={`bg-gray-800 rounded-xl shadow-2xl w-full flex flex-col border border-gray-700 ${sizeClasses}`}
        onClick={(e) => e.stopPropagation()}
      >
        <header className="flex justify-between items-center p-4 border-b border-gray-700">
          <h2 className="text-xl font-semibold text-teal-400">{title}</h2>
          <button
            onClick={onClose}
            className="text-gray-400 hover:text-white transition-colors p-2 rounded-full -ms-2"
            aria-label="بستن مودال"
          >
            <svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </header>
        <div className={`${contentPadding} overflow-y-auto flex-grow`}>
          {children}
        </div>
      </div>
    </div>
  );
};

export default Modal;