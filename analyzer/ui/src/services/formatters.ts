import moment from 'moment-jalaali';

moment.locale('fa'); // Set locale to Persian

export const formatDateTime = (dateString: string | null | Date): string => {
    if (!dateString) return 'نامشخص';
    try {
        return moment(dateString).format('HH:mm:ss jYYYY/jMM/jDD');
    } catch (e) {
        return 'تاریخ نامعتبر';
    }
};

export const formatTime = (dateString: string | null | Date): string => {
    if (!dateString) return 'نامشخص';
    try {
        return moment(dateString).format('HH:mm:ss');
    } catch (e) {
        return 'زمان نامعتبر';
    }
};

export const formatDate = (dateString: string | null | Date): string => {
    if (!dateString) return 'نامشخص';
    try {
        return moment(dateString).format('jYYYY/jMM/jDD');
    } catch (e) {
        return 'تاریخ نامعتبر';
    }
};

export const formatDuration = (seconds: number | null): string => {
    if (seconds === null || seconds < 0) return 'نامشخص';
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = Math.floor(seconds % 60);
    return [h, m, s].map(v => v.toString().padStart(2, '0')).join(':');
};

export const formatDurationForExport = (seconds: number | null): string => {
    if (seconds === null || seconds < 0) return 'نامشخص';
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = Math.floor(seconds % 60);
    return `${h} ساعت, ${m} دقیقه, ${s} ثانیه`;
};

export const formatForSheetName = (dateString: string | null | Date): string => {
    if (!dateString) return 'unknown_date';
    try {
        // Returns a safe format like '1403-05-09_14-30'
        return moment(dateString).format('jYYYY-jMM-jDD_HH-mm');
    } catch (e) {
        return 'invalid_date';
    }
};
