/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useCallback } from 'react';
import { Calendar, Printer, Search, FileText, BadgeHelp, RefreshCw, Layers, Bot, User, HelpCircle } from 'lucide-react';

export interface HistoryEvent {
  id: string;
  timestamp: string;
  dateString: string;
  machineId: string;
  machineName: string;
  status: 'EMPTY' | 'PLAN' | 'ALERT_ABSENT' | 'REPLACED';
  operator: string;
  code: string;
  source: 'WEB_MANUAL' | 'TELEGRAM' | 'SYSTEM_INIT';
}

interface DailyHistoryLogProps {
  lastDashboardUpdate: Date;
}

export const DailyHistoryLog: React.FC<DailyHistoryLogProps> = ({ lastDashboardUpdate }) => {
  const [selectedDate, setSelectedDate] = useState<string>('2026-05-29');
  const [logs, setLogs] = useState<HistoryEvent[]>([]);
  const [loading, setLoading] = useState<boolean>(false);
  const [errorMessage, setErrorMessage] = useState<string>('');

  const [showPrintModal, setShowPrintModal] = useState<boolean>(false);
  const [isInIframe, setIsInIframe] = useState<boolean>(false);

  // Detect whether we are in a sandbox iframe
  useEffect(() => {
    try {
      setIsInIframe(window.self !== window.top);
    } catch (e) {
      setIsInIframe(true);
    }
  }, []);

  const fetchHistory = useCallback(async (dateParam: string) => {
    setLoading(true);
    setErrorMessage('');
    try {
      const response = await fetch(`/api/history?date=${dateParam}`);
      if (!response.ok) {
        throw new Error(`Lỗi kết nối API: ${response.status}`);
      }
      const data = await response.json();
      // Sort events by timestamp ascending
      if (Array.isArray(data)) {
        const sorted = data.sort((a, b) => new Date(a.timestamp).getTime() - new Date(b.timestamp).getTime());
        setLogs(sorted);
      } else {
        setLogs([]);
      }
    } catch (err: any) {
      console.error(err);
      setErrorMessage(err.message || 'Không thể truy xuất dữ liệu');
    } finally {
      setLoading(false);
    }
  }, []);

  // Fetch log on dates changes, or when dashboard updates (to capture hot updates live)
  useEffect(() => {
    fetchHistory(selectedDate);
  }, [selectedDate, lastDashboardUpdate, fetchHistory]);

  const handlePrint = () => {
    if (isInIframe) {
      setShowPrintModal(true);
    } else {
      window.print();
    }
  };

  const openInNewTab = () => {
    window.open(window.location.href, '_blank');
  };

  const getStatusBadge = (status: HistoryEvent['status']) => {
    switch (status) {
      case 'PLAN':
        return <span className="px-2 py-0.5 rounded text-[10px] font-black bg-blue-50 text-blue-700 border border-blue-200">ĐANG ĐỨNG</span>;
      case 'ALERT_ABSENT':
        return <span className="px-2 py-0.5 rounded text-[10px] font-black bg-red-50 text-red-650 border border-red-200 animate-pulse">VẮNG MẶT</span>;
      case 'REPLACED':
        return <span className="px-2 py-0.5 rounded text-[10px] font-black bg-amber-50 text-amber-700 border border-amber-200">ĐÃ THAY THẾ</span>;
      case 'EMPTY':
      default:
        return <span className="px-2 py-0.5 rounded text-[10px] font-black bg-slate-100 text-slate-500 border border-slate-200">TRỐNG</span>;
    }
  };

  const getSourceBadge = (source: HistoryEvent['source']) => {
    if (source === 'TELEGRAM') {
      return (
        <span className="inline-flex items-center gap-1 text-[10px] bg-slate-50 text-slate-500 border border-slate-250 px-1.5 py-0.5 rounded font-bold">
          <Bot className="w-3.5 h-3.5 shrink-0" /> Đồng bộ cũ
        </span>
      );
    }
    return (
      <span className="inline-flex items-center gap-1 text-[10px] bg-slate-100 text-slate-600 border border-slate-200 px-1.5 py-0.5 rounded font-bold">
        <User className="w-3.5 h-3.5 shrink-0" /> Web (Tablet)
      </span>
    );
  };

  // Convert ISO string to local readable clock time
  const formatTime = (isoString: string) => {
    try {
      const d = new Date(isoString);
      return d.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    } catch {
      return '--:--:--';
    }
  };

  // Format YYYY-MM-DD back to readable Vietnamese date
  const formatReadableDate = (dateStr: string) => {
    try {
      const parts = dateStr.split('-');
      if (parts.length === 3) {
        return `Ngày ${parts[2]} tháng ${parts[1]} năm ${parts[0]}`;
      }
      return dateStr;
    } catch {
      return dateStr;
    }
  };

  return (
    <div id="factory-audit-section" className="w-full">
      
      {/* SCREEN VIEW (HIDDEN ON PRINTSTREAMS) */}
      <div className="bg-white border border-slate-200 rounded-2xl p-5 shadow-sm flex flex-col gap-4 print:hidden">
        
        {/* Module title header */}
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3 border-b border-slate-150 pb-3">
          <div className="flex items-center gap-2.5">
            <div className="p-2 bg-indigo-50 text-indigo-600 rounded-lg border border-indigo-100">
              <FileText className="w-5 h-5" />
            </div>
            <div>
              <h3 className="text-sm font-black text-slate-800 uppercase tracking-wider">
                Truy xuất Nhật ký & Điểm danh Thiết bị
              </h3>
              <p className="text-[11px] text-slate-500 font-medium font-semibold">
                Tra cứu phân công, vắng mặt của trực viên cơ giới theo ngày hoạt động và in ấn báo cáo PDF.
              </p>
            </div>
          </div>

          <div className="flex items-center gap-2 flex-wrap sm:flex-nowrap">
            {/* Quick date switches for easier testing */}
            <button
              onClick={() => setSelectedDate('2026-05-28')}
              className={`px-3 py-1 text-[10px] font-bold rounded-lg border transition ${
                selectedDate === '2026-05-28'
                  ? 'bg-indigo-600 border-indigo-500 text-white shadow-sm font-black'
                  : 'bg-slate-50 border-slate-205 text-slate-550 hover:text-slate-900 border-slate-200 hover:bg-slate-100'
              }`}
            >
              Hôm qua (28/05)
            </button>
            <button
              onClick={() => setSelectedDate('2026-05-29')}
              className={`px-3 py-1 text-[10px] font-bold rounded-lg border transition ${
                selectedDate === '2026-05-29'
                  ? 'bg-indigo-600 border-indigo-500 text-white shadow-sm font-black'
                  : 'bg-slate-50 border-slate-205 text-slate-550 hover:text-slate-900 border-slate-200 hover:bg-slate-100'
              }`}
            >
              Hôm nay (29/05)
            </button>
          </div>
        </div>

        {/* Dynamic selector form */}
        <div className="flex flex-col md:flex-row items-stretch md:items-center gap-3 bg-slate-50 p-4 rounded-xl border border-slate-200 shadow-inner">
          
          <div className="flex-1 flex items-center gap-3">
            <label className="text-[11px] font-black uppercase text-indigo-700 flex items-center gap-1.5 shrink-0">
              <Calendar className="w-4 h-4 text-indigo-650" />
              Chọn ngày truy xuất:
            </label>
            <input
              type="date"
              value={selectedDate}
              onChange={(e) => {
                if (e.target.value) {
                  setSelectedDate(e.target.value);
                }
              }}
              className="bg-white border border-slate-300 rounded-lg px-3 py-1.5 text-xs text-slate-800 uppercase font-black focus:outline-none focus:border-indigo-505 placeholder-slate-400 font-mono w-full max-w-[180px]"
            />
          </div>

          <div className="flex items-center gap-2 shrink-0">
            <button
              onClick={() => fetchHistory(selectedDate)}
              disabled={loading}
              className="px-3.5 py-1.5 bg-white hover:bg-slate-100 rounded-lg text-xs font-bold transition flex items-center gap-1.5 border border-slate-250 text-slate-700 disabled:opacity-50 cursor-pointer shadow-sm"
            >
              <RefreshCw className={`w-3.5 h-3.5 ${loading ? 'animate-spin' : ''}`} />
              Làm mới nhật ký
            </button>

            <button
              onClick={handlePrint}
              disabled={loading || logs.length === 0}
              className="px-4 py-1.5 bg-emerald-600 hover:bg-emerald-505 hover:bg-emerald-500 disabled:opacity-40 disabled:hover:bg-emerald-650 text-white rounded-lg text-xs font-black transition flex items-center gap-2 cursor-pointer shadow active:scale-95 transition-all"
            >
              <Printer className="w-4 h-4" />
              Xuất PDF / In Danh Sách
            </button>
          </div>

        </div>

        {/* Dynamic Display area */}
        {loading ? (
          <div className="py-12 flex flex-col items-center justify-center gap-2">
            <div className="w-8 h-8 border-3 border-slate-200 border-t-indigo-500 rounded-full animate-spin"></div>
            <p className="text-slate-550 text-xs font-semibold">Đang kéo nhật ký sản xuất...</p>
          </div>
        ) : errorMessage ? (
          <div className="p-4 bg-red-50 border border-red-200 rounded-xl text-center text-xs text-red-650 font-semibold">
            ⚠️ {errorMessage}
          </div>
        ) : logs.length === 0 ? (
          <div className="py-12 border border-dashed border-slate-200 rounded-xl flex flex-col items-center justify-center gap-3 text-center bg-slate-50/50 px-6">
            <div className="w-12 h-12 bg-slate-100 rounded-full flex items-center justify-center text-slate-500 border border-slate-200 shadow-sm">
              <BadgeHelp className="w-6 h-6 text-slate-400" />
            </div>
            <div>
              <p className="text-xs font-bold text-slate-755 text-slate-800">Không có bản ghi nhật ký nào</p>
              <p className="text-[10px] text-slate-450 mt-0.5 max-w-sm font-medium">
                Sơ đồ máy chưa nhận lệnh điều độ nào vào ngày {formatReadableDate(selectedDate)}. Quản trị viên có thể gán máy thủ công hoặc kích hoạt ca trực để tạo dữ liệu.
              </p>
            </div>
          </div>
        ) : (
          <div className="overflow-x-auto rounded-xl border border-slate-200 shadow-sm">
            <table className="w-full text-left text-xs border-collapse">
              <thead>
                <tr className="bg-slate-100 text-slate-600 uppercase tracking-widest text-[9px] font-black border-b border-slate-200">
                  <th className="py-3 px-4 w-12 text-center">STT</th>
                  <th className="py-3 px-4 w-28">Giờ điều động</th>
                  <th className="py-3 px-4">Vị trí máy thiết bị</th>
                  <th className="py-3 px-4">Nhân lực thực hiện (MSNV)</th>
                  <th className="py-3 px-4 w-40">Cách thức thay đổi</th>
                  <th className="py-3 px-4 w-32">Trạng thái mới</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-150 bg-white">
                {logs.map((log, index) => (
                  <tr key={log.id} className="hover:bg-slate-50/80 transition">
                    <td className="py-2.5 px-4 text-center font-mono text-slate-400 text-[10px]">{index + 1}</td>
                    <td className="py-2.5 px-4 font-mono font-bold text-indigo-700 whitespace-nowrap">{formatTime(log.timestamp)}</td>
                    <td className="py-2.5 px-4 font-black text-slate-800">{log.machineName}</td>
                    <td className="py-2.5 px-4 font-mono font-bold text-emerald-650">{log.code || log.operator || '--'}</td>
                    <td className="py-2.5 px-4">{getSourceBadge(log.source)}</td>
                    <td className="py-2.5 px-4 whitespace-nowrap">{getStatusBadge(log.status)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
            
            <div className="bg-slate-100 p-2 text-right text-[10px] text-slate-500 font-mono border-t border-slate-200 font-bold">
              Tổng số hành động gán và chấm công: <span className="text-indigo-600 font-extrabold">{logs.length}</span> bản ghi cho ngày {selectedDate}
            </div>
          </div>
        )}

      </div>


      {/* PRINT-ONLY CONTAINER (PURE PRO INDUSTRIAL BLACK-ON-WHITE REPORT LAYOUT FOR PDF EXPORT) */}
      <div className="hidden print:block bg-white text-black p-10 font-sans w-full min-h-screen text-[12px] leading-relaxed">
        
        {/* ISO Layout Header */}
        <div className="flex justify-between items-start border-b-2 border-black pb-5 mb-6">
          <div>
            <h4 className="text-[11px] font-black tracking-wider uppercase m-0 leading-tight">
              TỔNG CÔNG TY THỰC PHẨM WANT-WANT VN
            </h4>
            <h5 className="text-[10px] font-bold uppercase m-0 mt-0.5 text-gray-700">
              NHÀ MÁY SẢN XUẤT BÁNH GẠO SENBEI CHI NHÁNH I
            </h5>
            <p className="text-[9px] text-gray-650 m-0 mt-1 font-mono">
              ISO: 9001:2015 | Bộ phận Điều phối Cơ Giới Tự Động Hóa IIoT
            </p>
          </div>
          <div className="text-right">
            <span className="inline-block border border-black px-2 py-1 text-[9px] font-black uppercase">
              BẢN SÚC TRÌNH ĐIỀU ĐỘNG
            </span>
            <p className="text-[9px] m-0 mt-1 font-mono">MS-REP: {selectedDate.replace(/\-/g, '')}-WANT</p>
          </div>
        </div>

        {/* Auditing Document Title */}
        <div className="text-center my-6">
          <h2 className="text-xl font-extrabold tracking-tight uppercase border-b border-gray-350 pb-2 inline-block">
            BÁO CÁO ĐIỀU ĐỘNG NHÂN LỰC & TRẠNG THÁI THIẾT BỊ SẢN XUẤT
          </h2>
          <p className="text-xs font-bold italic text-gray-800 mt-2">
            ({formatReadableDate(selectedDate)})
          </p>
        </div>

        {/* Auditing Description Meta Fields */}
        <div className="grid grid-cols-2 gap-4 bg-gray-50 border border-black/15 p-4 rounded mb-6 text-xs font-semibold">
          <div>
            <p className="m-0"><strong>Hệ thống điều vận:</strong> Đồng bộ đám mây thời gian thực (Web Cloud)</p>
            <p className="m-0 mt-1"><strong>Người truy xuất báo cáo:</strong> Trực ban Trưởng Phân xưởng ({navigator.userAgent.substring(0, 30)}...)</p>
          </div>
          <div className="text-right">
            <p className="m-0"><strong>Thời điểm kết xuất PDF:</strong> {new Date().toLocaleString('vi-VN')}</p>
            <p className="m-0 mt-1"><strong>Tổng số sự kiện ghi nhận:</strong> {logs.length} lần biến chuyển trạng thái</p>
          </div>
        </div>

        {/* Printable events table */}
        <table className="w-full text-left text-xs border border-collapse border-black mb-10">
          <thead>
            <tr className="bg-gray-100 font-bold uppercase text-[10px] border-b border-black">
              <th className="border border-black py-2 px-3 text-center w-12">STT</th>
              <th className="border border-black py-2 px-3 w-32">Giờ Thay Đổi</th>
              <th className="border border-black py-2 px-3">Vị Trí Máy Thiết Bị</th>
              <th className="border border-black py-2 px-3">Mã Số Nhân Sự (MSNV)</th>
              <th className="border border-black py-2 px-3 w-36">Phương Thức Nhận Lệnh</th>
              <th className="border border-black py-2 px-3 w-36">Trạng Thái Kết Quả</th>
            </tr>
          </thead>
          <tbody>
            {logs.map((log, index) => {
              let label = 'Trống (Xám)';
              if (log.status === 'PLAN') label = 'Đăng ký Đứng máy';
              if (log.status === 'ALERT_ABSENT') label = 'Vắng mặt (Báo Đỏ)';
              if (log.status === 'REPLACED') label = 'Thay thế hoàn tất';

              let sourceLabel = log.source === 'TELEGRAM' ? 'Hệ thống (Telegram cũ)' : 'Bảng điều khiển Web';

              return (
                <tr key={log.id} className="border-b border-gray-300">
                  <td className="border border-black py-2 px-3 text-center">{index + 1}</td>
                  <td className="border border-black py-2 px-3 font-mono">{formatTime(log.timestamp)}</td>
                  <td className="border border-black py-2 px-3 font-bold">{log.machineName}</td>
                  <td className="border border-black py-2 px-3 font-mono">{log.code || log.operator || 'KĐK'}</td>
                  <td className="border border-black py-2 px-3 font-serif">{sourceLabel}</td>
                  <td className="border border-black py-2 px-3 font-bold uppercase">{label}</td>
                </tr>
              );
            })}
          </tbody>
        </table>

        {/* Signature blocks */}
        <div className="grid grid-cols-3 gap-6 text-center mt-12 text-xs">
          <div>
            <p className="font-bold underline uppercase">Kỹ sư vận hành giám sát</p>
            <p className="text-gray-500 italic mt-0.5">(Ký, ghi họ tên)</p>
            <div className="h-20" />
            <p className="font-bold text-gray-500">________________________</p>
          </div>
          <div>
            <p className="font-bold underline uppercase">Phòng QL Cơ Giới hóa IIoT</p>
            <p className="text-gray-500 italic mt-0.5">(Xác nhận hệ thống)</p>
            <div className="h-20" />
            <p className="font-mono text-[9px] text-emerald-650 font-black tracking-widest">System Verified ✓</p>
          </div>
          <div>
            <p className="font-bold underline uppercase">Trưởng Ca / Giám Đốc Phân Xưởng</p>
            <p className="text-gray-500 italic mt-0.5">(Phê duyệt lưu trữ)</p>
            <div className="h-20" />
            <p className="font-bold text-gray-500">________________________</p>
          </div>
        </div>

        {/* Footer legal disclaimer */}
        <div className="mt-16 text-center text-[9px] text-gray-500 border-t border-gray-350 pt-5 leading-relaxed">
          Báo cáo này được tự động thiết kế và trích xuất từ dữ liệu trực tuyến của nhà máy sản xuất gạo Want-Want Senbei chi nhánh IoT thông qua bộ ghi đệm `machines-db.json` & `history-db.json`. Chữ ký phê chuẩn hợp lệ cấu thành văn bản điểm danh chính thức của phân xưởng sản xuất.
        </div>

      </div>

      {/* PRINT PREVIEW / ISOLATION BYPASS MODAL */}
      {showPrintModal && (
        <div className="fixed inset-0 z-50 bg-slate-900/60 backdrop-blur-md flex items-center justify-center p-4 overflow-y-auto print:hidden animate-fade-in">
          <div className="bg-white border border-slate-300 rounded-2xl w-full max-w-4xl max-h-[90vh] flex flex-col shadow-2xl overflow-hidden">
            
            {/* Modal Header */}
            <div className="bg-slate-100 px-6 py-4 border-b border-slate-200 flex items-center justify-between">
              <div className="flex items-center gap-2">
                <Printer className="w-5 h-5 text-indigo-600 shrink-0" />
                <div>
                  <h3 className="text-sm font-black text-slate-950 uppercase tracking-wider">
                    Xem trước bản in &amp; Chỉ dẫn Xuất PDF
                  </h3>
                  <p className="text-[10px] text-emerald-650 font-bold">
                    Công cụ hỗ trợ duyệt hồ sơ ISO-SENBEI Want-Want
                  </p>
                </div>
              </div>
              <button
                onClick={() => setShowPrintModal(false)}
                className="text-xs font-black text-slate-655 hover:text-slate-900 bg-slate-200 hover:bg-slate-250 py-1.5 px-3 rounded-lg transition cursor-pointer"
              >
                Đóng Xem Trước (ESC)
              </button>
            </div>

            {/* Modal Body Container with scrollable visual check */}
            <div className="p-6 overflow-y-auto flex-1 space-y-6">
              
              {/* Informational Guidance Warning Panel */}
              <div className="bg-indigo-50 border border-indigo-200 p-4 rounded-xl space-y-3">
                <div className="flex items-start gap-3">
                  <span className="text-xl shrink-0 mt-0.5">💡</span>
                  <div>
                    <h4 className="text-xs font-extrabold text-indigo-850 uppercase tracking-wide">
                      Hướng dẫn in / Lưu file PDF trong môi trường Sandbox
                    </h4>
                    <p className="text-[11px] text-slate-705 leading-relaxed mt-1">
                      Do tính năng xem trước ứng dụng của AI Studio chạy bên trong một <strong className="text-indigo-850">iFrame Bảo vệ</strong>, trình duyệt sẽ tự động <strong className="text-red-650">chặn và khóa</strong> hộp thoại in hệ thống của bạn (lỗi Security Sandbox).
                    </p>
                    <p className="text-[11px] text-slate-705 leading-relaxed mt-1">
                      Để thực hiện in ấn hoặc chọn <strong className="text-emerald-700">&quot;Lưu dưới dạng PDF (Save as PDF)&quot;</strong> thành công 100%, bạn chỉ cần thực hiện 2 bước đơn giản dưới đây:
                    </p>
                  </div>
                </div>

                <div className="bg-white p-3 rounded-lg border border-indigo-200 text-xs text-indigo-900 space-y-1 shadow-sm">
                  <p className="font-semibold text-[11px]">👉 Bước 1: Nhấn nút màu xanh <strong className="text-indigo-600">&quot;Mở Tab Mới Để In&quot;</strong> bên dưới để đưa ứng dụng ra môi trường an toàn độc lập.</p>
                  <p className="font-semibold text-[11px]">👉 Bước 2: Tại Tab mới, nhấn nút <strong className="text-emerald-600">&quot;Xuất PDF / In Danh Sách&quot;</strong> lần nữa để in/lưu PDF lập tức!</p>
                </div>

                <div className="flex flex-wrap gap-2.5 pt-1.5">
                  <button
                    onClick={openInNewTab}
                    className="px-4 py-2 bg-gradient-to-r from-indigo-600 to-blue-600 hover:from-indigo-500 hover:to-blue-500 text-white rounded-xl text-xs font-black transition flex items-center justify-center gap-2 cursor-pointer shadow-lg active:scale-95"
                  >
                    <Layers className="w-4 h-4 shrink-0" />
                    BƯỚC 1: MỞ TAB MỚI ĐỂ IN (AN TOÀN)
                  </button>
                  <button
                    onClick={() => {
                      setShowPrintModal(false);
                      setTimeout(() => window.print(), 150);
                    }}
                    className="px-4 py-2 bg-slate-100 hover:bg-slate-205 border border-slate-300 text-slate-700 font-semibold rounded-xl text-xs transition cursor-pointer"
                  >
                    Vẫn thử in tại Tab này
                  </button>
                </div>
              </div>

              {/* Virtual A4 Paper document preview rendering */}
              <div>
                <h5 className="text-[10px] font-black text-slate-505 uppercase tracking-widest mb-2.5 flex items-center gap-1.5">
                  <span>📄</span> XEM TRƯỚC SÚC TRÌNH ISO A4 (THỰC TẾ TRANG IN):
                </h5>
                
                <div className="border border-slate-300 rounded-xl bg-white text-black p-8 shadow-inner overflow-x-auto select-none">
                  <div className="min-w-[650px] font-sans text-[11px] leading-relaxed">
                    
                    {/* A4 ISO Header */}
                    <div className="flex justify-between items-start border-b-2 border-black pb-4 mb-4">
                      <div>
                        <h4 className="text-[10px] font-black tracking-wider uppercase m-0 leading-tight text-gray-900">
                          TỔNG CÔNG TY THỰC PHẨM WANT-WANT VN
                        </h4>
                        <h5 className="text-[9px] font-bold uppercase m-0 mt-0.5 text-gray-600">
                          NHÀ MÁY SẢN XUẤT BÁNH GẠO SENBEI CHI NHÁNH I
                        </h5>
                        <p className="text-[8px] text-gray-500 m-0 mt-0.5 font-mono">
                          ISO: 9001:2015 | Bộ phận Điều phối Cơ Giới Tự Động Hóa IIoT
                        </p>
                      </div>
                      <div className="text-right">
                        <span className="inline-block border border-black px-1.5 py-0.5 text-[8px] font-black uppercase text-gray-900">
                          BẢN SÚC TRÌNH ĐIỀU ĐỘNG
                        </span>
                        <p className="text-[8px] m-0 mt-0.5 font-mono text-gray-600">MS-REP: {selectedDate.replace(/\-/g, '')}-WANT</p>
                      </div>
                    </div>

                    {/* A4 Doc Title */}
                    <div className="text-center my-4">
                      <h2 className="text-base font-extrabold tracking-tight uppercase border-b border-gray-300 pb-1.5 inline-block text-black">
                        BÁO CÁO ĐIỀU ĐỘNG NHÂN LỰC &amp; TRẠNG THÁI THIẾT BỊ SẢN XUẤT
                      </h2>
                      <p className="text-[10px] font-bold italic text-gray-700 mt-1">
                        ({formatReadableDate(selectedDate)})
                      </p>
                    </div>

                    {/* Active events summary report */}
                    <table className="w-full text-left text-[10px] border border-collapse border-black mb-6">
                      <thead>
                        <tr className="bg-gray-100 font-bold uppercase text-[9px] border-b border-black text-gray-900">
                          <th className="border border-black py-1.5 px-2 text-center w-10">STT</th>
                          <th className="border border-black py-1.5 px-2 w-24">Giờ Thay Đổi</th>
                          <th className="border border-black py-1.5 px-2">Vị Trí Máy Thiết Bị</th>
                          <th className="border border-black py-1.5 px-2">Mã Số Nhân Sự (MSNV)</th>
                          <th className="border border-black py-1.5 px-2">Phương Thức Nhận Lệnh</th>
                          <th className="border border-black py-1.5 px-2">Trạng Thái Kết Quả</th>
                        </tr>
                      </thead>
                      <tbody>
                        {logs.map((log, index) => {
                          let label = 'Trống (Xám)';
                          if (log.status === 'PLAN') label = 'Đăng ký Đứng máy';
                          if (log.status === 'ALERT_ABSENT') label = 'Vắng mặt (Báo Đỏ)';
                          if (log.status === 'REPLACED') label = 'Thay thế hoàn tất';

                          let sourceLabel = log.source === 'TELEGRAM' ? 'Hệ thống (Telegram cũ)' : 'Bảng điều khiển Web';

                          return (
                            <tr key={log.id} className="border-b border-gray-300 text-gray-800">
                              <td className="border border-black py-1 px-2 text-center">{index + 1}</td>
                              <td className="border border-black py-1 px-2 font-mono">{formatTime(log.timestamp)}</td>
                              <td className="border border-black py-1 px-2 font-bold">{log.machineName}</td>
                              <td className="border border-black py-1 px-2 font-mono">{log.code || log.operator || 'KĐK'}</td>
                              <td className="border border-black py-1 px-2">{sourceLabel}</td>
                              <td className="border border-black py-1 px-2 font-bold uppercase">{label}</td>
                            </tr>
                          );
                        })}
                      </tbody>
                    </table>

                    {/* Tiny Signature placeholders */}
                    <div className="grid grid-cols-3 gap-4 text-center mt-6 text-[9px] text-gray-900">
                      <div>
                        <p className="font-bold underline uppercase">Kỹ sư vận hành giám sát</p>
                        <div className="h-10" />
                        <p className="font-bold text-gray-400">________________________</p>
                      </div>
                      <div>
                        <p className="font-bold underline uppercase">Phòng QL Cơ Giới hóa IIoT</p>
                        <div className="h-10" />
                        <p className="font-mono text-[8px] text-cyan-800 font-bold">System Verified ✓</p>
                      </div>
                      <div>
                        <p className="font-bold underline uppercase">Giám Đốc Phân Xưởng</p>
                        <div className="h-10" />
                        <p className="font-bold text-gray-400">________________________</p>
                      </div>
                    </div>

                  </div>
                </div>
              </div>

            </div>

            {/* Modal Footer actions */}
            <div className="bg-slate-50 px-6 py-4 border-t border-slate-205 flex items-center justify-end gap-3">
              <button
                onClick={() => setShowPrintModal(false)}
                className="px-5 py-2 bg-slate-100 hover:bg-slate-200 text-slate-700 border border-slate-200 rounded-xl text-xs font-bold transition cursor-pointer shadow-sm"
              >
                Đóng lại
              </button>
            </div>

          </div>
        </div>
      )}

    </div>
  );
};
