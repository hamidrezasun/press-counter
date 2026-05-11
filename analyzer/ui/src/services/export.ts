import * as XLSX from 'xlsx';

const sanitizeSheetName = (name: string): string => {
    // Excel sheet names must be <= 31 chars and cannot contain: \ / ? * [ ] :
    const forbiddenChars = /[\\/?*[\]:]/g;
    const sanitized = name.replace(forbiddenChars, '_');
    return sanitized.substring(0, 31);
};

export const exportToExcel = (sheets: { name: string; data: any[] }[], fileName: string) => {
    try {
        const workbook = XLSX.utils.book_new();
        sheets.forEach(sheetInfo => {
            const safeSheetName = sanitizeSheetName(sheetInfo.name);
            const worksheet = XLSX.utils.json_to_sheet(sheetInfo.data);
            XLSX.utils.book_append_sheet(workbook, worksheet, safeSheetName);
        });
        XLSX.writeFile(workbook, `${fileName}.xlsx`);
    } catch (error) {
        console.error("Failed to export to Excel", error);
        alert("خطا در خروجی گرفتن اکسل.");
    }
};