import React from 'react';

interface CardProps {
  children: React.ReactNode;
  className?: string;
  onClick?: () => void;
}

const Card: React.FC<CardProps> = ({ children, className = '', onClick }) => {
  const baseClasses = "bg-gray-800 rounded-lg shadow-lg p-6 border border-gray-700 flex flex-col";
  const interactiveClasses = onClick ? "transition-all duration-300 ease-in-out hover:shadow-teal-500/20 hover:border-teal-500/50 hover:-translate-y-1 cursor-pointer" : "";

  return (
    <div className={`${baseClasses} ${interactiveClasses} ${className}`} onClick={onClick}>
      {children}
    </div>
  );
};

export default Card;