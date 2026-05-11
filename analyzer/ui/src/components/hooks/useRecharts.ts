import { useState, useEffect } from 'react';

const useRecharts = () => {
    const [recharts, setRecharts] = useState<any | null>(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        const checkRecharts = () => {
            if ((window as any).Recharts) {
                setRecharts((window as any).Recharts);
                setLoading(false);
            } else {
                // Poll for the Recharts object to appear on the window
                const interval = setInterval(() => {
                    if ((window as any).Recharts) {
                        setRecharts((window as any).Recharts);
                        setLoading(false);
                        clearInterval(interval);
                    }
                }, 100);

                return () => clearInterval(interval);
            }
        };

        // Check immediately
        checkRecharts();

        // Also listen for the script load event as a fallback
        const script = document.querySelector('script[src*="recharts"]');
        if (script) {
            script.addEventListener('load', checkRecharts);
            return () => script.removeEventListener('load', checkRecharts);
        }
    }, []);

    return { Recharts: recharts, loading };
};

export default useRecharts;
