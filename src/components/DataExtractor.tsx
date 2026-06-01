/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useMemo, useCallback } from 'react';
import { 
  Calendar, Download, FileSpreadsheet, FileJson, Copy, Check, 
  Table, History, Filter, Search, RotateCcw, 
  CheckCircle2, AlertCircle, RefreshCw, Layers, Printer, ArrowDown, HelpCircle, FileText, Ban,
  Lock, User, UserPlus, LogIn, LogOut, ShieldCheck, ShieldAlert, Key, Unlock
} from 'lucide-react';
import { Machine } from '../types';

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

interface DataExtractorProps {
  machines: Machine[];
  onRefreshMachines: () => void;
}

export const DataExtractor: React.FC<DataExtractorProps> = ({ machines, onRefreshMachines }) => {
  // Extraction mode: 'current' (Instant snapshot), 'history' (Historical audit logs), or 'admins' (Admin accounts management)
  const [extractMode, setExtractMode] = useState<'current' | 'history' | 'admins'>('current');

  // Admin Login / Registration states
  const [isAdminLoggedIn, setIsAdminLoggedIn] = useState<string | null>(() => {
    try {
      return localStorage.getItem('admin-username');
    } catch {
      return null;
    }
  });
  
  const [authMode, setAuthMode] = useState<'login' | 'register'>('login');
  const [authUsername, setAuthUsername] = useState<string>('');
  const [authPassword, setAuthPassword] = useState<string>('');
  const [authConfirmPassword, setAuthConfirmPassword] = useState<string>('');
  const [authLoading, setAuthLoading] = useState<boolean>(false);
  const [authError, setAuthError] = useState<string>('');
  const [authSuccess, setAuthSuccess] = useState<string>('');

  const handleAdminLogin = async (e: React.FormEvent) => {
    e.preventDefault();
    setAuthError('');
    setAuthSuccess('');
    
    if (!authUsername.trim() || !authPassword.trim()) {
      setAuthError('Vui lòng điền đầy đủ thông tin đăng nhập!');
      return;
    }
    
    setAuthLoading(true);
    try {
      const res = await fetch('/api/admin/login', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          username: authUsername.trim(),
          password: authPassword.trim()
        })
      });
      
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.error || 'Đăng nhập không thành công');
      }
      
      localStorage.setItem('admin-username', data.admin.username);
      setIsAdminLoggedIn(data.admin.username);
      setAuthPassword('');
      setAuthUsername('');
    } catch (err: any) {
      setAuthError(err.message || 'Lỗi kết nối đến máy chủ xác thực.');
    } finally {
      setAuthLoading(false);
    }
  };

  const handleAdminRegister = async (e: React.FormEvent) => {
    e.preventDefault();
    setAuthError('');
    setAuthSuccess('');
    
    if (!authUsername.trim() || !authPassword.trim() || !authConfirmPassword.trim()) {
      setAuthError('Vui lòng điền đầy đủ tất cả các trường!');
      return;
    }
    
    if (authPassword !== authConfirmPassword) {
      setAuthError('Mật khẩu và xác nhận mật khẩu không trùng khớp!');
      return;
    }
    
    setAuthLoading(true);
    try {
      const res = await fetch('/api/admin/register', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          username: authUsername.trim(),
          password: authPassword.trim()
        })
      });
      
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.error || 'Đăng ký không thành công');
      }
      
      setAuthSuccess('Đăng ký tài khoản quản trị thành công! Xin hãy đăng nhập với tài khoản mới này.');
      setAuthMode('login');
      setAuthPassword('');
      setAuthConfirmPassword('');
    } catch (err: any) {
      setAuthError(err.message || 'Lỗi kết nối máy chủ khi tạo tài khoản.');
    } finally {
      setAuthLoading(false);
    }
  };

  const handleAdminLogout = () => {
    localStorage.removeItem('admin-username');
    setIsAdminLoggedIn(null);
    setAuthError('');
    setAuthSuccess('');
  };

  // Admin Account Approval Dashboard States
  const [adminsList, setAdminsList] = useState<any[]>([]);
  const [adminsLoading, setAdminsLoading] = useState<boolean>(false);
  const [adminsError, setAdminsError] = useState<string>('');
  const [actionLoadingId, setActionLoadingId] = useState<string | null>(null);

  const fetchAdminsList = async () => {
    if (!isAdminLoggedIn) return;
    setAdminsLoading(true);
    setAdminsError('');
    try {
      const res = await fetch(`/api/admin/list?requester=${encodeURIComponent(isAdminLoggedIn)}`);
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.error || 'Không tải được danh sách tài khoản');
      }
      setAdminsList(data);
    } catch (err: any) {
      setAdminsError(err.message || 'Lỗi kết nối máy chủ.');
    } finally {
      setAdminsLoading(false);
    }
  };

  useEffect(() => {
    if (extractMode === 'admins' && isAdminLoggedIn) {
      fetchAdminsList();
    }
  }, [extractMode, isAdminLoggedIn]);

  const handleApproveAdmin = async (targetUsername: string) => {
    if (!isAdminLoggedIn) return;
    setActionLoadingId(targetUsername);
    try {
      const res = await fetch('/api/admin/approve', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ requester: isAdminLoggedIn, target: targetUsername })
      });
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.error || 'Phê duyệt thất bại');
      }
      await fetchAdminsList();
    } catch (err: any) {
      alert(err.message || 'Có lỗi xảy ra');
    } finally {
      setActionLoadingId(null);
    }
  };

  const handleDeleteAdmin = async (targetUsername: string) => {
    if (!isAdminLoggedIn) return;
    if (!window.confirm(`Bạn có chắc chắn muốn từ chối/xóa tài khoản "${targetUsername}" khỏi hệ thống?`)) {
      return;
    }
    setActionLoadingId(targetUsername);
    try {
      const res = await fetch('/api/admin/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ requester: isAdminLoggedIn, target: targetUsername })
      });
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.error || 'Xóa tài khoản thất bại');
      }
      await fetchAdminsList();
    } catch (err: any) {
      alert(err.message || 'Có lỗi xảy ra');
    } finally {
      setActionLoadingId(null);
    }
  };
  
  // Status feedback states
  const [copied, setCopied] = useState<boolean>(false);
  const [copiedHistory, setCopiedHistory] = useState<boolean>(false);
  
  // 1. Snapshot screen states
  const [snapshotSearch, setSnapshotSearch] = useState<string>('');
  const [snapshotStatusFilter, setSnapshotStatusFilter] = useState<string>('ALL');
  
  // 2. History screen states
  const [selectedDate, setSelectedDate] = useState<string>(() => {
    // Default to today
    const d = new Date();
    const tzOffset = d.getTimezoneOffset() * 60000;
    const localISOTime = (new Date(Date.now() - tzOffset)).toISOString().slice(0, 10);
    return localISOTime;
  });
  const [historyLogs, setHistoryLogs] = useState<HistoryEvent[]>([]);
  const [historyLoading, setHistoryLoading] = useState<boolean>(false);
  const [historyError, setHistoryError] = useState<string>('');
  const [historySearch, setHistorySearch] = useState<string>('');
  const [historySourceFilter, setHistorySourceFilter] = useState<string>('ALL');
  const [historyStatusFilter, setHistoryStatusFilter] = useState<string>('ALL');
  const [confirmClear, setConfirmClear] = useState<boolean>(false);

  // Fetch History Logs
  const fetchHistory = useCallback(async (dateParam: string) => {
    setHistoryLoading(true);
    setHistoryError('');
    try {
      const response = await fetch(`/api/history?date=${dateParam}`);
      if (!response.ok) {
        throw new Error(`Lỗi kết nối API: ${response.status}`);
      }
      const data = await response.json();
      if (Array.isArray(data)) {
        const sorted = data.sort((a, b) => new Date(a.timestamp).getTime() - new Date(b.timestamp).getTime());
        setHistoryLogs(sorted);
      } else {
        setHistoryLogs([]);
      }
    } catch (err: any) {
      console.error(err);
      setHistoryError(err.message || 'Không thể truy xuất dữ liệu lịch sử');
    } finally {
      setHistoryLoading(false);
    }
  }, []);

  const handleClearData = async () => {
    if (!confirmClear) {
      setConfirmClear(true);
      return;
    }

    try {
      setHistoryLoading(true);
      const response = await fetch('/api/admin/clear-data', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          requester: isAdminLoggedIn,
          date: selectedDate
        })
      });

      const data = await response.json();
      if (!response.ok) {
        throw new Error(data.error || 'Xoá dữ liệu thất bại');
      }

      alert(data.message || 'Xoá dữ liệu thành công');
      setConfirmClear(false);
      
      // Refresh data
      fetchHistory(selectedDate);
      onRefreshMachines();
    } catch (err: any) {
      alert(err.message || 'Lỗi khi kết nối đến máy chủ để xoá dữ liệu');
    } finally {
      setHistoryLoading(false);
    }
  };

  // Fetch log on select changes
  useEffect(() => {
    if (extractMode === 'history') {
      fetchHistory(selectedDate);
    }
  }, [selectedDate, extractMode, fetchHistory]);

  // Clean Vietnamese labels for statuses
  const getStatusLabel = (status: string) => {
    switch (status) {
      case 'PLAN': return 'Có người đứng (PLAN)';
      case 'ALERT_ABSENT': return 'Vắng mặt (ABSENT)';
      case 'REPLACED': return 'Thay thế (REPLACED)';
      case 'EMPTY': return 'Chờ gán / Trống';
      default: return status;
    }
  };

  // Convert status code to style classes
  const getStatusColor = (status: string) => {
    switch (status) {
      case 'PLAN': return 'bg-blue-50 text-blue-700 border border-blue-200';
      case 'ALERT_ABSENT': return 'bg-red-50 text-red-650 border border-red-200';
      case 'REPLACED': return 'bg-amber-50 text-amber-700 border border-amber-200';
      case 'EMPTY':
      default:
        return 'bg-slate-100 text-slate-500 border border-slate-200';
    }
  };

  // 1. FILTERING CURRENT SNAPSHOT DATA
  const filteredSnapshot = useMemo(() => {
    return machines.filter((m) => {
      // Filter status
      if (snapshotStatusFilter !== 'ALL' && m.status !== snapshotStatusFilter) {
        return false;
      }
      // Filter search
      if (snapshotSearch.trim() !== '') {
        const q = snapshotSearch.toLowerCase();
        const nameMatch = m.name?.toLowerCase().includes(q);
        const codeMatch = m.id?.toLowerCase().includes(q);
        const opMatch = m.operator?.toLowerCase().includes(q);
        return nameMatch || codeMatch || opMatch;
      }
      return true;
    });
  }, [machines, snapshotStatusFilter, snapshotSearch]);

  // 2. FILTERING HISTORICAL DATA
  const filteredHistory = useMemo(() => {
    return historyLogs.filter((log) => {
      // Source filter
      if (historySourceFilter !== 'ALL' && log.source !== historySourceFilter) {
        return false;
      }
      // Status filter
      if (historyStatusFilter !== 'ALL' && log.status !== historyStatusFilter) {
        return false;
      }
      // Search term
      if (historySearch.trim() !== '') {
        const q = historySearch.toLowerCase();
        const nameMatch = log.machineName?.toLowerCase().includes(q);
        const idMatch = log.machineId?.toLowerCase().includes(q);
        const opMatch = log.operator?.toLowerCase().includes(q);
        const codeMatch = log.code?.toLowerCase().includes(q);
        return nameMatch || idMatch || opMatch || codeMatch;
      }
      return true;
    });
  }, [historyLogs, historySourceFilter, historyStatusFilter, historySearch]);

  // Aggregate stats on active filtered history
  const historyStats = useMemo(() => {
    const total = filteredHistory.length;
    const absents = filteredHistory.filter(l => l.status === 'ALERT_ABSENT').length;
    const telegramActions = filteredHistory.filter(l => l.source === 'TELEGRAM').length;
    const webActions = filteredHistory.filter(l => l.source === 'WEB_MANUAL').length;
    
    // Unique operators active today
    const uniqueOps = new Set<string>();
    filteredHistory.forEach(l => {
      if (l.operator) uniqueOps.add(l.operator);
      if (l.code) uniqueOps.add(l.code);
    });

    return { total, absents, telegramActions, webActions, uniqueOperators: uniqueOps.size };
  }, [filteredHistory]);

  // Snapshot aggregate statistics
  const snapshotStats = useMemo(() => {
    const total = machines.length;
    const active = machines.filter(m => m.status === 'PLAN' || m.status === 'REPLACED').length;
    const absent = machines.filter(m => m.status === 'ALERT_ABSENT').length;
    const empty = machines.filter(m => m.status === 'EMPTY' || !m.status).length;
    return { total, active, absent, empty };
  }, [machines]);

  // FORMAT TIME
  const formatTime = (isoString: string) => {
    try {
      const d = new Date(isoString);
      return d.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    } catch {
      return '--:--:--';
    }
  };

  // EXPORT CURRENT SNAPSHOT TO CSV
  const handleExportSnapshotCSV = () => {
    const headers = ['Vị Trí Máy (ID)', 'Tên Thiết Bị', 'Mã Số Nhân Sự', 'Họ Tên Nhân Sự', 'Trạng Thái Vận Hành', 'Cập Nhật Cuối'];
    const rows = filteredSnapshot.map(m => [
      m.id,
      m.name,
      m.operator || '',
      m.operator ? `Thành Viên ${m.operator}` : '',
      m.status,
      m.updated_at ? new Date(m.updated_at).toLocaleString('vi-VN') : ''
    ]);

    const csvContent = "\uFEFF" + [headers.join(','), ...rows.map(e => e.map(val => `"${val.replace(/"/g, '""')}"`).join(','))].join('\n');
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.setAttribute("href", url);
    link.setAttribute("download", `SENBEI_Snapshot_Production_${new Date().toISOString().slice(0, 10)}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  // EXPORT CURRENT SNAPSHOT TO JSON
  const handleExportSnapshotJSON = () => {
    const jsonString = JSON.stringify(filteredSnapshot, null, 2);
    const blob = new Blob([jsonString], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.setAttribute("href", url);
    link.setAttribute("download", `SENBEI_Snapshot_Data_${new Date().toISOString().slice(0,10)}.json`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  // COPY CURRENT SNAPSHOT TABLE AS MARKDOWN / TSV TO CLIPBOARD FOR EXCEL PASTE
  const handleCopySnapshotToClipboard = () => {
    let text = "Vị Trí Máy (ID)\tTên Thiết Bị\tNhân Sự Trực Ban\tTrạng Thái\n";
    filteredSnapshot.forEach(m => {
      text += `${m.id}\t${m.name}\t${m.operator || 'TRỐNG'}\t${m.status}\n`;
    });
    navigator.clipboard.writeText(text).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  // EXPORT HISTORICAL EVENTS TO CSV
  const handleExportHistoryCSV = () => {
    const headers = ['Số Thứ Tự', 'Thời Gian Ghi Nhận', 'Mã Vị Trí Máy', 'Tên Thiết Bị', 'Mã Số Nhân Viên', 'Trạng Thái', 'Nguồn Điều Động'];
    const rows = filteredHistory.map((log, index) => [
      index + 1,
      new Date(log.timestamp).toLocaleString('vi-VN'),
      log.machineId,
      log.machineName,
      log.code || log.operator || 'Trống',
      log.status,
      log.source === 'TELEGRAM' ? 'Telegram Bot' : 'Màn hình Tablet'
    ]);

    const csvContent = "\uFEFF" + [headers.join(','), ...rows.map(e => e.map(val => `"${String(val).replace(/"/g, '""')}"`).join(','))].join('\n');
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.setAttribute("href", url);
    link.setAttribute("download", `SENBEI_LichSuGD_Ngay_${selectedDate}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  // EXPORT HISTORICAL LOGS TO JSON
  const handleExportHistoryJSON = () => {
    const jsonString = JSON.stringify(filteredHistory, null, 2);
    const blob = new Blob([jsonString], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.setAttribute("href", url);
    link.setAttribute("download", `SENBEI_History_${selectedDate}.json`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  // COPY HISTORY TO CLIPBOARD FOR EXCEL
  const handleCopyHistory = () => {
    let text = "STT\tThời Gian\tVị Trí Thiết Bị\tMã Nhân Viên\tNguồn Cập Nhật\tTrạng Thái Mới\n";
    filteredHistory.forEach((log, idx) => {
      text += `${idx + 1}\t${formatTime(log.timestamp)}\t${log.machineName}\t${log.code || log.operator || '--'}\t${log.source}\t${log.status}\n`;
    });
    navigator.clipboard.writeText(text).then(() => {
      setCopiedHistory(true);
      setTimeout(() => setCopiedHistory(false), 2000);
    });
  };

  if (!isAdminLoggedIn) {
    return (
      <div className="max-w-md mx-auto my-8 bg-white border border-slate-200 rounded-2xl shadow-xl overflow-hidden animate-fade-in text-left">
        {/* Top vibrant card accent */}
        <div className="h-2 bg-gradient-to-r from-indigo-600 via-indigo-500 to-emerald-500" />
        
        <div className="p-6 md:p-8">
          {/* Header branding */}
          <div className="flex flex-col items-center text-center gap-3 mb-6">
            <div className="p-3.5 bg-indigo-50 border border-indigo-200 rounded-2xl text-indigo-600 shadow-inner">
              <Lock className="w-8 h-8 animate-pulse" />
            </div>
            <div>
              <h2 className="text-lg font-black text-slate-900 tracking-tight uppercase flex items-center gap-1.5 justify-center">
                <span>Xác thực Quản trị viên</span>
              </h2>
              <p className="text-xs text-slate-500 mt-1 font-semibold leading-relaxed">
                Vui lòng đăng nhập hoặc đăng ký tài khoản quản lý phân xưởng để sử dụng các công cụ trích xuất dữ liệu.
              </p>
            </div>
          </div>

          {/* Tab switches */}
          <div className="flex bg-slate-100 p-1 rounded-xl mb-6 border border-slate-200">
            <button
              type="button"
              onClick={() => {
                setAuthMode('login');
                setAuthError('');
                setAuthSuccess('');
              }}
              className={`flex-1 py-1.5 text-xs font-black uppercase tracking-wider rounded-lg transition-all cursor-pointer ${
                authMode === 'login'
                  ? 'bg-white text-indigo-700 shadow border border-slate-200/50'
                  : 'text-slate-500 hover:text-slate-800'
              }`}
            >
              Đăng Nhập
            </button>
            <button
              type="button"
              onClick={() => {
                setAuthMode('register');
                setAuthError('');
                setAuthSuccess('');
              }}
              className={`flex-1 py-1.5 text-xs font-black uppercase tracking-wider rounded-lg transition-all cursor-pointer ${
                authMode === 'register'
                  ? 'bg-white text-indigo-700 shadow border border-slate-200/50'
                  : 'text-slate-500 hover:text-slate-800'
              }`}
            >
              Đăng Ký
            </button>
          </div>

          {/* Feedback states */}
          {authError && (
            <div className="p-3 bg-red-50 border border-red-200 rounded-xl mb-5 flex items-start gap-2.5 text-xs text-red-650 font-bold shadow-sm animate-fade-in">
              <ShieldAlert className="w-4 h-4 text-red-600 shrink-0 mt-0.5" />
              <span>{authError}</span>
            </div>
          )}

          {authSuccess && (
            <div className="p-3 bg-emerald-50 border border-emerald-200 rounded-xl mb-5 flex items-start gap-2.5 text-xs text-emerald-750 font-bold shadow-sm animate-fade-in">
              <ShieldCheck className="w-4 h-4 text-emerald-600 shrink-0 mt-0.5" />
              <span>{authSuccess}</span>
            </div>
          )}

          {/* Forms */}
          {authMode === 'login' ? (
            <form onSubmit={handleAdminLogin} className="space-y-4">
              <div className="space-y-1.5">
                <label className="text-[10px] font-black uppercase text-slate-500 tracking-wider block">Tên Tài Khoản</label>
                <div className="relative">
                  <span className="absolute inset-y-0 left-0 flex items-center pl-3.5 pointer-events-none text-slate-400">
                    <User className="w-4 h-4" />
                  </span>
                  <input
                    type="text"
                    required
                    placeholder="Nhập tên đăng nhập..."
                    value={authUsername}
                    onChange={(e) => setAuthUsername(e.target.value)}
                    className="w-full bg-slate-50 border border-slate-200 rounded-xl py-2.5 pl-10 pr-4 text-xs font-semibold focus:outline-none focus:border-indigo-500 text-slate-800 placeholder-slate-400 shadow-inner"
                  />
                </div>
              </div>

              <div className="space-y-1.5">
                <label className="text-[10px] font-black uppercase text-slate-500 tracking-wider block">Mật Khẩu</label>
                <div className="relative">
                  <span className="absolute inset-y-0 left-0 flex items-center pl-3.5 pointer-events-none text-slate-400">
                    <Key className="w-4 h-4" />
                  </span>
                  <input
                    type="password"
                    required
                    placeholder="••••••••"
                    value={authPassword}
                    onChange={(e) => setAuthPassword(e.target.value)}
                    className="w-full bg-slate-50 border border-slate-200 rounded-xl py-2.5 pl-10 pr-4 text-xs font-semibold focus:outline-none focus:border-indigo-500 text-slate-800 placeholder-slate-400 shadow-inner"
                  />
                </div>
              </div>

              <button
                type="submit"
                disabled={authLoading}
                className="w-full mt-2 py-3 bg-indigo-600 hover:bg-indigo-700 disabled:opacity-50 text-white font-black text-xs uppercase tracking-wider rounded-xl transition-all shadow-md hover:shadow-lg flex items-center justify-center gap-2 cursor-pointer active:scale-[0.98]"
              >
                {authLoading ? (
                  <div className="w-4 h-4 border-2 border-white/30 border-t-white rounded-full animate-spin" />
                ) : (
                  <LogIn className="w-4 h-4" />
                )}
                {authLoading ? 'Đang xác thực bảo mật...' : 'Đăng Nhập'}
              </button>
            </form>
          ) : (
            <form onSubmit={handleAdminRegister} className="space-y-4">
              <div className="space-y-1.5">
                <label className="text-[10px] font-black uppercase text-slate-500 tracking-wider block">Đăng ký Tên Tài Khoản</label>
                <div className="relative">
                  <span className="absolute inset-y-0 left-0 flex items-center pl-3.5 pointer-events-none text-slate-400">
                    <User className="w-4 h-4" />
                  </span>
                  <input
                    type="text"
                    required
                    placeholder="Ít nhất 3 ký tự..."
                    value={authUsername}
                    onChange={(e) => setAuthUsername(e.target.value)}
                    className="w-full bg-slate-50 border border-slate-200 rounded-xl py-2.5 pl-10 pr-4 text-xs font-semibold focus:outline-none focus:border-indigo-500 text-slate-800 placeholder-slate-400 shadow-inner"
                  />
                </div>
              </div>

              <div className="space-y-1.5">
                <label className="text-[10px] font-black uppercase text-slate-500 tracking-wider block">Mật Khẩu Mới</label>
                <div className="relative">
                  <span className="absolute inset-y-0 left-0 flex items-center pl-3.5 pointer-events-none text-slate-400">
                    <Key className="w-4 h-4" />
                  </span>
                  <input
                    type="password"
                    required
                    placeholder="Ít nhất 4 ký tự..."
                    value={authPassword}
                    onChange={(e) => setAuthPassword(e.target.value)}
                    className="w-full bg-slate-50 border border-slate-200 rounded-xl py-2.5 pl-10 pr-4 text-xs font-semibold focus:outline-none focus:border-indigo-500 text-slate-800 placeholder-slate-400 shadow-inner"
                  />
                </div>
              </div>

              <div className="space-y-1.5">
                <label className="text-[10px] font-black uppercase text-slate-500 tracking-wider block">Xác nhận Mật khẩu mới</label>
                <div className="relative">
                  <span className="absolute inset-y-0 left-0 flex items-center pl-3.5 pointer-events-none text-slate-400">
                    <Unlock className="w-4 h-4" />
                  </span>
                  <input
                    type="password"
                    required
                    placeholder="Nhập lại mật khẩu..."
                    value={authConfirmPassword}
                    onChange={(e) => setAuthConfirmPassword(e.target.value)}
                    className="w-full bg-slate-50 border border-slate-200 rounded-xl py-2.5 pl-10 pr-4 text-xs font-semibold focus:outline-none focus:border-indigo-500 text-slate-800 placeholder-slate-400 shadow-inner"
                  />
                </div>
              </div>

              <button
                type="submit"
                disabled={authLoading}
                className="w-full mt-2 py-3 bg-gradient-to-r from-indigo-600 to-indigo-700 hover:from-indigo-700 hover:to-indigo-800 disabled:opacity-50 text-white font-black text-xs uppercase tracking-wider rounded-xl transition-all shadow-md hover:shadow-lg flex items-center justify-center gap-2 cursor-pointer active:scale-[0.98]"
              >
                {authLoading ? (
                  <div className="w-4 h-4 border-2 border-white/30 border-t-white rounded-full animate-spin" />
                ) : (
                  <UserPlus className="w-4 h-4" />
                )}
                {authLoading ? 'Đang tạo quản trị...' : 'Tạo Tài Khoản Admin'}
              </button>
            </form>
          )}

          {/* Quick Guidance credentials */}
          <div className="mt-8 pt-5 border-t border-slate-100 flex items-start gap-2.5 bg-slate-50/50 p-2.5 border border-dashed rounded-xl">
            <HelpCircle className="w-4.5 h-4.5 text-indigo-500 shrink-0 mt-0.5" />
            <div className="text-[10.5px] text-slate-600 font-semibold leading-relaxed">
              <span className="font-bold text-indigo-700">Tài khoản mẫu ban đầu:</span>
              <div className="mt-1 font-mono text-[10px]">
                <div>• ID: <span className="font-black text-slate-800 select-all">admin</span></div>
                <div>• Pass: <span className="font-black text-slate-800 select-all">admin123</span></div>
              </div>
              <p className="mt-1.5 text-[9.5px] text-slate-400">
                Ủy ban Giám sát phân xưởng Senbei IIoT.
              </p>
            </div>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      
      {/* Tab select header buttons - Professional styled for Wanna-Wants Senbei Factory UI */}
      <div className="flex flex-col lg:flex-row lg:items-center justify-between gap-4 bg-slate-100 p-3 border border-slate-205 rounded-2xl shadow-sm">
        <div className="flex flex-wrap items-center gap-2">
          <button
            onClick={() => setExtractMode('current')}
            className={`flex items-center gap-2 px-4 py-2 text-xs font-black uppercase tracking-wider rounded-xl transition-all cursor-pointer ${
              extractMode === 'current'
                ? 'bg-gradient-to-r from-indigo-600 to-indigo-700 text-white shadow-lg border border-indigo-500/30'
                : 'text-slate-600 hover:text-slate-900 bg-white hover:bg-slate-50 border border-slate-205 shadow-sm'
            }`}
          >
            <Table className="w-4 h-4" />
            Trích xuất Trạng thái Hiện tại
          </button>
          
          <button
            onClick={() => setExtractMode('history')}
            className={`flex items-center gap-2 px-4 py-2 text-xs font-black uppercase tracking-wider rounded-xl transition-all cursor-pointer ${
              extractMode === 'history'
                ? 'bg-gradient-to-r from-indigo-600 to-indigo-700 text-white shadow-lg border border-indigo-500/30'
                : 'text-slate-600 hover:text-slate-900 bg-white hover:bg-slate-50 border border-slate-205 shadow-sm'
            }`}
          >
            <History className="w-4 h-4" />
            Trích xuất Lịch Sử Điểm Danh ({selectedDate.split('-').reverse().slice(0, 2).join('/')})
          </button>

          <button
            onClick={() => setExtractMode('admins')}
            className={`flex items-center gap-2 px-4 py-2 text-xs font-black uppercase tracking-wider rounded-xl transition-all cursor-pointer ${
              extractMode === 'admins'
                ? 'bg-gradient-to-r from-indigo-600 to-indigo-700 text-white shadow-lg border border-indigo-500/30'
                : 'text-slate-600 hover:text-slate-900 bg-white hover:bg-slate-50 border border-slate-205 shadow-sm'
            }`}
          >
            <Key className="w-4 h-4" />
            Phê duyệt Tài khoản Admin
          </button>
        </div>

        <div className="flex items-center gap-3">
          <div className="text-[10px] text-slate-600 bg-white border border-slate-250 px-2.5 py-1.5 rounded-xl flex items-center gap-1.5 shadow-sm">
            <ShieldCheck className="w-3.5 h-3.5 text-emerald-600" />
            <span className="font-semibold">Quản trị:</span>
            <span className="font-black text-slate-800">{isAdminLoggedIn}</span>
          </div>
          
          <button
            type="button"
            onClick={handleAdminLogout}
            className="flex items-center gap-1 bg-red-50 hover:bg-red-100 border border-red-200 rounded-xl px-2.5 py-1.5 text-[10px] font-black text-red-650 tracking-wide uppercase transition-all duration-150 cursor-pointer shadow-sm hover:shadow"
            title="Đăng xuất khỏi bảng quản lý"
          >
            <LogOut className="w-3 h-3" />
            Đăng xuất
          </button>
        </div>
      </div>

      {/* ----------------- MODE A: CURRENT STATE EXTRACTOR ----------------- */}
      {extractMode === 'current' && (
        <div className="bg-white border border-slate-200 rounded-2xl p-5 shadow-sm flex flex-col gap-5 animate-fade-in text-left">
          
          <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 border-b border-slate-100 pb-4">
            <div>
              <h2 className="text-base font-black text-slate-900 uppercase tracking-wider flex items-center gap-2">
                <span className="w-2.5 h-2.5 rounded-full bg-indigo-600 animate-pulse" />
                Dữ Liệu Trạng Thái Điều Độ Hiện Tại
              </h2>
              <p className="text-xs text-slate-500 mt-1 font-medium">
                Xuất thông tin của 23 vị trí sản xuất trên dây chuyền theo lượt trực ban thời gian thực.
              </p>
            </div>

            {/* General snapshot metrics */}
            <div className="flex items-center gap-3 text-[11px] font-mono font-semibold">
              <div className="bg-slate-50 px-3 py-1.5 border border-slate-200 rounded-xl text-slate-700">
                Tổng Máy: <span className="text-indigo-600 font-black">{snapshotStats.total}</span>
              </div>
              <div className="bg-slate-50 px-3 py-1.5 border border-slate-200 rounded-xl text-slate-700">
                Có Người: <span className="text-blue-600 font-black">{snapshotStats.active}</span>
              </div>
              <div className="bg-slate-50 px-3 py-1.5 border border-slate-200 rounded-xl text-slate-700">
                Vắng Mặt: <span className="text-red-655 text-red-600 font-black">{snapshotStats.absent}</span>
              </div>
              <div className="bg-slate-50 px-3 py-1.5 border border-slate-200 rounded-xl text-slate-500">
                Thiếu Trực: <span className="text-slate-400 font-black">{snapshotStats.empty}</span>
              </div>
            </div>
          </div>

          {/* Controls: Search, Filter, Export */}
          <div className="grid grid-cols-1 md:grid-cols-12 gap-3 items-center">
            
            {/* Search Input */}
            <div className="md:col-span-5 relative">
              <span className="absolute inset-y-0 left-0 flex items-center pl-3 pointer-events-none text-slate-400">
                <Search className="w-4 h-4" />
              </span>
              <input
                type="text"
                placeholder="Tìm kiếm nhanh Trực viên, Thiết bị, Mã máy..."
                value={snapshotSearch}
                onChange={(e) => setSnapshotSearch(e.target.value)}
                className="w-full bg-slate-50 border border-slate-200 rounded-xl py-2 pl-9 pr-4 text-xs font-semibold focus:outline-none focus:border-indigo-500 text-slate-800 placeholder-slate-400 placeholder:font-medium font-sans shadow-inner"
              />
            </div>

            {/* Status Filter Dropdown */}
            <div className="md:col-span-3">
              <div className="flex items-center bg-slate-50 border border-slate-200 rounded-xl px-2.5 py-1.5 shadow-sm">
                <span className="text-[10px] uppercase font-black text-slate-450 pr-1 select-none">Lọc:</span>
                <select
                  value={snapshotStatusFilter}
                  onChange={(e) => setSnapshotStatusFilter(e.target.value)}
                  className="bg-transparent text-xs font-bold text-slate-700 focus:outline-none flex-1 cursor-pointer"
                >
                  <option value="ALL">Tất cả Trạng Thái</option>
                  <option value="PLAN">Có người (PLAN)</option>
                  <option value="ALERT_ABSENT">Vắng mặt (ABSENT)</option>
                  <option value="REPLACED">Thay thế (REPLACED)</option>
                  <option value="EMPTY">Chưa phân công (EMPTY)</option>
                </select>
              </div>
            </div>

            {/* Quick Reset Button if filters exist */}
            <div className="md:col-span-1">
              {(snapshotSearch !== '' || snapshotStatusFilter !== 'ALL') && (
                <button
                  onClick={() => {
                    setSnapshotSearch('');
                    setSnapshotStatusFilter('ALL');
                  }}
                  className="p-2 bg-slate-50 hover:bg-slate-100 text-slate-500 hover:text-slate-800 rounded-xl border border-slate-200 flex items-center justify-center transition shadow-sm cursor-pointer"
                  title="Nhập lại bộ lọc"
                >
                  <RotateCcw className="w-4 h-4" />
                </button>
              )}
            </div>

            {/* Export Actions Trigger block */}
            <div className="md:col-span-3 flex items-center justify-end gap-2">
              <button
                onClick={handleCopySnapshotToClipboard}
                className="px-3 py-2 bg-slate-50 hover:bg-slate-100 border border-slate-200 rounded-xl text-xs font-bold flex items-center gap-1.5 transition text-slate-600 hover:text-slate-900 cursor-pointer shadow-sm"
                title="Sao chép dạng bảng văn bản để dán vào Excel"
              >
                {copied ? <Check className="w-4 h-4 text-emerald-600" /> : <Copy className="w-4 h-4 text-indigo-600" />}
                {copied ? 'Đã sao chép' : 'Copy Excel'}
              </button>

              <button
                onClick={handleExportSnapshotCSV}
                className="px-3 py-2 bg-emerald-600 hover:bg-emerald-700 text-white border border-emerald-500/20 rounded-xl text-xs font-black flex items-center gap-1.5 transition cursor-pointer shadow-md"
              >
                <FileSpreadsheet className="w-4 h-4" />
                Xuất CSV
              </button>

              <button
                onClick={handleExportSnapshotJSON}
                className="p-2 bg-indigo-50 hover:bg-indigo-100 border border-indigo-200 text-indigo-600 rounded-xl text-xs font-bold flex items-center justify-center transition cursor-pointer shadow-sm"
                title="Xuất định dạng cấu trúc JSON"
              >
                <FileJson className="w-4.5 h-4.5 text-indigo-600" />
              </button>
            </div>
          </div>

          {/* Active Filtering Snapshot Results Table */}
          {filteredSnapshot.length === 0 ? (
            <div className="py-20 text-center border border-dashed border-slate-200 rounded-2xl bg-slate-50 px-4">
              <p className="text-slate-500 text-xs font-bold">Không tìm thấy thiết bị nào trùng khớp bộ lọc</p>
              <p className="text-[11px] text-slate-400 mt-1">Vui lòng thử điều chỉnh lại từ khóa tìm kiếm hoặc bấm nút Thiết lập lại.</p>
            </div>
          ) : (
            <div className="overflow-x-auto rounded-xl border border-slate-200 max-h-[480px] shadow-sm">
              <table className="w-full text-left text-xs border-collapse">
                <thead>
                  <tr className="bg-slate-105 text-slate-500 uppercase tracking-wider text-[9px] font-black border-b border-slate-200 sticky top-0">
                    <th className="py-3 px-4 w-12 text-center">STT</th>
                    <th className="py-3 px-4 w-32">ID Thiết Bị</th>
                    <th className="py-3 px-4">Tên Trạm Phân Công</th>
                    <th className="py-3 px-4">Nhân sự Đang trực</th>
                    <th className="py-3 px-4 w-44">Tình trạng Phân xưởng</th>
                    <th className="py-3 px-4 w-40">Cập nhật lúc</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-100 bg-white">
                  {filteredSnapshot.map((m, idx) => (
                    <tr key={m.id} className="hover:bg-slate-50 transition">
                      <td className="py-2 px-4 text-center text-[10px] font-mono text-slate-400">{idx + 1}</td>
                      <td className="py-2 px-4 font-mono font-bold text-slate-500">{m.id}</td>
                      <td className="py-2 px-4 font-black text-slate-800">{m.name}</td>
                      <td className="py-2 px-4 font-mono font-black">
                        {m.operator ? (
                          <span className="px-2 py-0.5 bg-amber-400 text-slate-950 font-bold rounded text-[10px] shadow-sm">
                            {m.operator}
                          </span>
                        ) : (
                          <span className="text-slate-350 font-bold italic">TRỐNG / CHỜ GÁN</span>
                        )}
                      </td>
                      <td className="py-2 px-4">
                        <span className={`px-2 py-0.5 rounded text-[10px] font-black tracking-wide ${getStatusColor(m.status)}`}>
                          {m.status ? getStatusLabel(m.status) : 'TRỐNG'}
                        </span>
                      </td>
                      <td className="py-2 px-4 text-[10px] text-slate-400 font-mono">
                        {m.updated_at ? new Date(m.updated_at).toLocaleTimeString('vi-VN') : '--:--:--'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>

              <div className="bg-slate-50 p-2.5 text-right font-mono text-[10px] text-slate-500 border-t border-slate-200 flex justify-between">
                <span>Trực ban hiện hữu: {filteredSnapshot.filter(m => m.operator).length} trạm trực</span>
                <span>Tuyển tập được <span className="text-indigo-600 font-bold">{filteredSnapshot.length}</span> / {machines.length} vị trí</span>
              </div>
            </div>
          )}

        </div>
      )}

      {/* ----------------- MODE B: HISTORICAL ASSIGNMENT AUDIT LOGS ----------------- */}
      {extractMode === 'history' && (
        <div className="bg-white border border-slate-200 rounded-2xl p-5 shadow-sm flex flex-col gap-5 animate-fade-in text-left">
          
          <div className="flex flex-col lg:flex-row lg:items-center justify-between gap-4 border-b border-slate-100 pb-4">
            <div>
              <h2 className="text-base font-black text-slate-900 uppercase tracking-wider flex items-center gap-2">
                <span className="w-2.5 h-2.5 rounded-full bg-emerald-500 animate-pulse" />
                Lịch Sử Phân Công & Trích Xuất Chấm Công
              </h2>
              <p className="text-xs text-slate-500 mt-1 font-medium">
                Quản lý, tìm kiếm và chiết xuất nhật ký hành trình điều động, sắp ca và phân công nhân viên sản xuất.
              </p>
            </div>

            {/* Quick date picker */}
            <div className="flex flex-wrap items-center gap-2 bg-slate-50 p-2 border border-slate-200 rounded-xl shadow-sm">
              <span className="text-[10px] uppercase font-black text-slate-450 pl-1">Ngày truy xuất:</span>
              <input
                type="date"
                value={selectedDate}
                onChange={(e) => {
                  if (e.target.value) setSelectedDate(e.target.value);
                }}
                className="bg-white border border-slate-200 rounded-lg px-2.5 py-1 text-xs text-slate-800 uppercase font-black focus:outline-none focus:border-indigo-500 font-mono shadow-sm"
              />
              <button
                onClick={() => fetchHistory(selectedDate)}
                className="p-1 px-2.5 bg-white hover:bg-slate-100 border border-slate-200 rounded-lg text-xs font-black text-slate-650 hover:text-slate-900 transition flex items-center gap-1 cursor-pointer shadow-sm"
                title="Tải lại bảng ngày chọn"
              >
                <RefreshCw className={`w-3 h-3 text-indigo-600 ${historyLoading ? 'animate-spin' : ''}`} />
                Kiểm tra
              </button>

              <button
                onClick={handleClearData}
                onBlur={() => setConfirmClear(false)}
                className={`p-1 px-2.5 border rounded-lg text-xs font-black transition flex items-center gap-1 cursor-pointer shadow-sm ${
                  confirmClear
                    ? 'bg-red-600 border-red-500 text-white hover:bg-red-700'
                    : 'bg-red-50 hover:bg-red-100 border-red-200 text-red-650 hover:text-red-700'
                }`}
                title="Xoá toàn bộ lịch sử điểm danh và kế hoạch ca của ngày này"
              >
                <Ban className="w-3 h-3" />
                {confirmClear ? 'Xác nhận xoá?' : 'Xoá dữ liệu'}
              </button>
            </div>
          </div>

          {/* Multi-metrics Summary Cards */}
          <div className="grid grid-cols-2 md:grid-cols-4 gap-3.5">
            <div className="bg-slate-50 p-3 rounded-xl border border-slate-200 text-left shadow-sm">
              <span className="text-[10px] text-slate-500 font-bold uppercase block tracking-wider">Tổng Hoạt Động</span>
              <span className="text-xl font-black text-slate-800 font-mono mt-1 block">{historyStats.total} <span className="text-xs text-slate-400 font-normal">lượt</span></span>
            </div>

            <div className="bg-slate-50 p-3 rounded-xl border border-slate-200 text-left shadow-sm">
              <span className="text-[10px] text-slate-500 font-bold uppercase block tracking-wider">Nhân Sự Ghi Nhận</span>
              <span className="text-xl font-black text-emerald-600 font-mono mt-1 block">{historyStats.uniqueOperators} <span className="text-xs text-slate-400 font-normal">người</span></span>
            </div>

            <div className="bg-slate-50 p-3 rounded-xl border border-slate-200 text-left shadow-sm">
              <span className="text-[10px] text-slate-500 font-bold uppercase block tracking-wider">Vắng Mặt Báo Cáo</span>
              <span className="text-xl font-black text-red-600 font-mono mt-1 block">{historyStats.absents} <span className="text-xs text-slate-400 font-normal">lượt</span></span>
            </div>

            <div className="bg-slate-50 p-3 rounded-xl border border-slate-200 text-left shadow-sm">
              <span className="text-[10px] text-slate-500 font-bold uppercase block tracking-wider">Hoạt Động Trên Web</span>
              <span className="text-xl font-black text-blue-600 font-mono mt-1 block">{historyStats.webActions} <span className="text-xs text-slate-400 font-normal">lượt</span></span>
            </div>
          </div>

          {/* Audit Search and Multi Filter row */}
          <div className="grid grid-cols-1 md:grid-cols-12 gap-3 items-center">
            
            {/* Search within log */}
            <div className="md:col-span-4 relative">
              <span className="absolute inset-y-0 left-0 flex items-center pl-3 pointer-events-none text-slate-400">
                <Search className="w-4 h-4" />
              </span>
              <input
                type="text"
                placeholder="Lọc mã/tên người trực, vị trí máy móc..."
                value={historySearch}
                onChange={(e) => setHistorySearch(e.target.value)}
                className="w-full bg-slate-50 border border-slate-200 rounded-xl py-2 pl-9 pr-4 text-xs font-semibold focus:outline-none focus:border-indigo-500 text-slate-800 placeholder-slate-400 font-sans shadow-inner"
              />
            </div>

            {/* Source Filter Dropdown */}
            <div className="md:col-span-2.5">
              <div className="flex items-center bg-slate-50 border border-slate-200 rounded-xl px-2.5 py-1.5 shadow-sm">
                <span className="text-[10px] uppercase font-black text-slate-450 pr-1 shrink-0">Phun:</span>
                <select
                  value={historySourceFilter}
                  onChange={(e) => setHistorySourceFilter(e.target.value)}
                  className="bg-transparent text-xs font-bold text-slate-705 focus:outline-none flex-1 cursor-pointer"
                >
                  <option value="ALL">Tất cả Nguồn</option>
                  <option value="WEB_MANUAL">Giao diện Web</option>
                </select>
              </div>
            </div>

            {/* Status Filter Dropdown */}
            <div className="md:col-span-2.5">
              <div className="flex items-center bg-slate-50 border border-slate-200 rounded-xl px-2.5 py-1.5 shadow-sm">
                <span className="text-[10px] uppercase font-black text-slate-450 pr-1 shrink-0">Mức:</span>
                <select
                  value={historyStatusFilter}
                  onChange={(e) => setHistoryStatusFilter(e.target.value)}
                  className="bg-transparent text-xs font-bold text-slate-705 focus:outline-none flex-1 cursor-pointer"
                >
                  <option value="ALL">Tất cả Trạng Thái</option>
                  <option value="PLAN">Có người (PLAN)</option>
                  <option value="ALERT_ABSENT">Vắng mặt (ABSENT)</option>
                  <option value="REPLACED">Thay thế (REPLACED)</option>
                  <option value="EMPTY">Để Trống / Giải phóng</option>
                </select>
              </div>
            </div>

            {/* Reset Logs Filter */}
            <div className="md:col-span-0.5 flex justify-center">
              {(historySearch !== '' || historySourceFilter !== 'ALL' || historyStatusFilter !== 'ALL') && (
                <button
                  onClick={() => {
                    setHistorySearch('');
                    setHistorySourceFilter('ALL');
                    setHistoryStatusFilter('ALL');
                  }}
                  className="p-1.5 bg-slate-50 hover:bg-slate-100 text-slate-500 hover:text-slate-850 rounded-xl border border-slate-202 flex items-center justify-center transition shadow-sm cursor-pointer"
                  title="Xóa bộ lọc lịch sử"
                >
                  <RotateCcw className="w-3.5 h-3.5" />
                </button>
              )}
            </div>

            {/* Export and PDF actions block */}
            <div className="md:col-span-2.5 flex items-center justify-end gap-1.5">
              <button
                onClick={handleCopyHistory}
                disabled={filteredHistory.length === 0}
                className="px-2.5 py-1.5 bg-slate-50 hover:bg-slate-100 disabled:opacity-50 border border-slate-200 rounded-lg text-xs font-bold flex items-center gap-1 transition text-slate-600 hover:text-slate-900 cursor-pointer shadow-sm"
                title="Sao chép toàn bộ nhật ký"
              >
                {copiedHistory ? <Check className="w-3.5 h-3.5 text-emerald-650 font-black" /> : <Copy className="w-3.5 h-3.5 text-indigo-600" />}
                Copy Excel
              </button>

              <button
                onClick={handleExportHistoryCSV}
                disabled={filteredHistory.length === 0}
                className="px-2.5 py-1.5 bg-emerald-600 hover:bg-emerald-700 disabled:opacity-50 text-white border border-emerald-500/25 rounded-lg text-xs font-extrabold flex items-center gap-1 transition cursor-pointer shadow-md"
              >
                <FileSpreadsheet className="w-3.5 h-3.5" />
                Tải CSV
              </button>
            </div>

          </div>

          {/* Database querying responses */}
          {historyLoading ? (
            <div className="py-24 flex flex-col items-center justify-center gap-2">
              <div className="w-9 h-9 border-3 border-slate-200 border-t-indigo-600 rounded-full animate-spin"></div>
              <p className="text-slate-500 text-xs font-semibold">Đang tiến hành truy vấn cơ sở dữ liệu lịch sử...</p>
            </div>
          ) : historyError ? (
            <div className="py-12 bg-red-50 border border-red-200 rounded-2xl text-center text-xs text-red-650 font-bold">
              ⚠️ Gặp lỗi khi trích xuất dữ liệu: {historyError}
            </div>
          ) : filteredHistory.length === 0 ? (
            <div className="py-16 border border-dashed border-slate-200 rounded-2xl flex flex-col items-center justify-center gap-3 bg-slate-50 text-center px-4 shadow-inner">
              <div className="p-3 bg-white rounded-full border border-slate-200 text-slate-400 shadow-sm">
                <Ban className="w-6 h-6 text-slate-405" />
              </div>
              <div>
                <p className="text-xs font-bold text-slate-700">Không có dữ liệu nhật ký sản phẩm trùng khớp</p>
                <p className="text-[11px] text-slate-400 mt-0.5 font-medium">
                  Không ghi nhận hoạt động nào trong ngày {selectedDate.split('-').reverse().join('/')} cho các bộ lọc đang chọn.
                </p>
              </div>
            </div>
          ) : (
            <div className="overflow-x-auto rounded-xl border border-slate-200 max-h-[460px] shadow-sm">
              <table className="w-full text-left text-xs border-collapse">
                <thead>
                  <tr className="bg-slate-105 text-slate-500 uppercase tracking-wider text-[9px] font-black border-b border-slate-200 sticky top-0">
                    <th className="py-3 px-4 w-12 text-center">STT</th>
                    <th className="py-3 px-4 w-28">Giờ ghi nhận</th>
                    <th className="py-3 px-4">Mã vị trí máy</th>
                    <th className="py-3 px-4">Tên Thiết Bị Phân Chuyền</th>
                    <th className="py-3 px-4">Nhân Lực Điểm Danh</th>
                    <th className="py-3 px-4 w-36">Kênh Truyền</th>
                    <th className="py-3 px-4 w-36">Trạng Thái Đổi</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-100 bg-white">
                  {filteredHistory.map((log, index) => (
                    <tr key={log.id} className="hover:bg-slate-50 transition">
                      <td className="py-2 px-4 text-center font-mono text-[10px] text-slate-400">{index + 1}</td>
                      <td className="py-2 px-4 font-mono font-bold text-indigo-600">{formatTime(log.timestamp)}</td>
                      <td className="py-2 px-4 font-mono text-[10px] text-slate-400">{log.machineId}</td>
                      <td className="py-2 px-4 font-black text-slate-800">{log.machineName}</td>
                      <td className="py-2 px-4 font-mono font-black text-emerald-600">
                        {log.code || log.operator ? (
                          <span className="px-1.5 py-0.5 bg-slate-50 rounded border border-slate-200 text-slate-700 font-bold text-[11px] shadow-sm">
                            {log.code || log.operator}
                          </span>
                        ) : '--'}
                      </td>
                      <td className="py-2 px-4 whitespace-nowrap">
                        {log.source === 'TELEGRAM' ? (
                          <span className="inline-flex items-center gap-0.5 text-[9px] font-bold bg-slate-100 text-slate-500 border border-slate-250 px-1.5 py-0.5 rounded shadow-sm">
                            Hệ thống cũ
                          </span>
                        ) : (
                          <span className="inline-flex items-center gap-0.5 text-[9px] font-bold bg-slate-50 text-slate-600 border border-slate-200 px-1.5 py-0.5 rounded shadow-sm">
                            Giao diện Web
                          </span>
                        )}
                      </td>
                      <td className="py-2 px-4 whitespace-nowrap">
                        <span className={`px-2 py-0.5 rounded-md text-[9px] font-black tracking-wide ${getStatusColor(log.status)}`}>
                          {getStatusLabel(log.status)}
                        </span>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>

              <div className="bg-slate-50 p-2.5 text-right font-mono text-[10px] text-slate-500 border-t border-slate-200">
                Chiết xuất thành công <span className="text-indigo-600 font-bold">{filteredHistory.length}</span> / {historyLogs.length} sự kiện điều phái.
              </div>
            </div>
          )}

          {/* Quick guide on how to ingest data to see history */}
          <div className="bg-indigo-50/50 border border-indigo-100 rounded-xl p-3 flex items-start gap-2.5 shadow-inner">
            <HelpCircle className="w-4 h-4 text-indigo-600 shrink-0 mt-0.5 font-bold" />
            <p className="text-[11px] text-indigo-700 leading-relaxed font-semibold">
              <b>Mẹo vận hành:</b> Hành động phân công, xếp ca trực tiếp trên màn hình quản lý hoặc kích hoạt ca trực đều được ghi nhận vào luồng lịch sử của ngày tương ứng để hỗ trợ truy soát tỷ lệ chuyên cần và điểm danh.
            </p>
          </div>

        </div>
      )}

      {/* ----------------- MODE C: ADMINS MANAGEMENT DASHBOARD ----------------- */}
      {extractMode === 'admins' && (
        <div className="bg-white border border-slate-200 rounded-2xl p-5 shadow-sm flex flex-col gap-5 animate-fade-in text-left">
          <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 border-b border-slate-100 pb-4">
            <div>
              <h2 className="text-base font-black text-slate-900 uppercase tracking-wider flex items-center gap-2">
                <ShieldCheck className="w-5 h-5 text-indigo-600" />
                Phê Duyệt & Quản Lý Tài Khoản Quản Trị Viên
              </h2>
              <p className="text-xs text-slate-500 mt-1 font-semibold leading-relaxed">
                Xử lý kích hoạt tài khoản của các nhân sự vừa đăng ký. Các tài khoản mới tạo buộc phải được tài khoản hoạt động phê duyệt để có thể đăng nhập.
              </p>
            </div>
            
            <button
              onClick={fetchAdminsList}
              disabled={adminsLoading}
              className="px-3 py-1.5 bg-slate-50 hover:bg-slate-100 disabled:opacity-50 border border-slate-200 rounded-xl text-xs font-bold flex items-center gap-1.5 transition text-slate-700 cursor-pointer shadow-sm ml-auto sm:ml-0"
              title="Làm mới danh sách"
            >
              <RefreshCw className={`w-3.5 h-3.5 text-indigo-600 ${adminsLoading ? 'animate-spin' : ''}`} />
              Làm mới trang
            </button>
          </div>

          {adminsLoading ? (
            <div className="py-20 flex flex-col items-center justify-center gap-2">
              <div className="w-8 h-8 border-2 border-slate-200 border-t-indigo-600 rounded-full animate-spin"></div>
              <p className="text-slate-500 text-xs font-semibold">Đang truy vấn danh sách tài khoản quản trị từ máy chủ...</p>
            </div>
          ) : adminsError ? (
            <div className="py-10 bg-red-50 border border-red-200 rounded-2xl text-center text-xs text-red-650 font-bold">
              ⚠️ Có lỗi xảy ra: {adminsError}
            </div>
          ) : adminsList.length === 0 ? (
            <div className="py-14 border border-dashed border-slate-200 rounded-2xl flex flex-col items-center justify-center gap-2 bg-slate-50 text-center">
              <Ban className="w-6 h-6 text-slate-400" />
              <p className="text-xs font-bold text-slate-700">Chưa ghi nhận tài khoản quản trị nào.</p>
            </div>
          ) : (
            <div className="overflow-x-auto rounded-xl border border-slate-200 shadow-sm">
              <table className="w-full text-left text-xs border-collapse">
                <thead>
                  <tr className="bg-slate-105 text-slate-500 uppercase tracking-wider text-[9px] font-black border-b border-slate-200">
                    <th className="py-3 px-4 w-12 text-center font-black">STT</th>
                    <th className="py-3 px-4">Tên quản trị viên</th>
                    <th className="py-3 px-4 w-44">Thời điểm mở tài khoản</th>
                    <th className="py-3 px-4 w-36 text-center">Trạng thái duyệt</th>
                    <th className="py-3 px-4 w-44">Người phê duyệt</th>
                    <th className="py-3 px-4 w-48 text-right">Lệnh vận hành</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-100 bg-white">
                  {adminsList.map((adm, index) => {
                    const isSelf = adm.username.toLowerCase() === isAdminLoggedIn?.toLowerCase();
                    const isDefaultAdmin = adm.username.toLowerCase() === 'admin';
                    
                    return (
                      <tr key={adm.username} className={`hover:bg-slate-50 transition duration-150 ${!adm.approved ? 'bg-amber-50/20' : ''}`}>
                        <td className="py-3 px-4 text-center font-mono text-[10px] text-slate-400 font-bold">{index + 1}</td>
                        <td className="py-3 px-4 font-black text-slate-800">
                          <div className="flex items-center gap-1.5 flex-wrap">
                            <span className="font-bold text-slate-900">{adm.username}</span>
                            {isSelf && (
                              <span className="px-1.5 py-0.5 bg-indigo-50 text-indigo-600 border border-indigo-200 text-[8px] font-black uppercase rounded">TÔI</span>
                            )}
                            {isDefaultAdmin && (
                              <span className="px-1.5 py-0.5 bg-amber-50 text-amber-700 border border-amber-150 text-[8px] font-black uppercase rounded">ROOT</span>
                            )}
                          </div>
                        </td>
                        <td className="py-3 px-4 font-mono text-[10.5px] text-slate-500 font-semibold whitespace-nowrap">
                          {adm.createdAt ? new Date(adm.createdAt).toLocaleString('vi-VN', { 
                            hour: '2-digit', 
                            minute: '2-digit',
                            day: '2-digit', 
                            month: '2-digit', 
                            year: 'numeric' 
                          }) : '--'}
                        </td>
                        <td className="py-3 px-4 text-center">
                          {adm.approved ? (
                            <span className="inline-flex items-center gap-0.5 px-2 py-0.5 bg-emerald-50 text-emerald-700 border border-emerald-100 rounded text-[9px] font-black uppercase tracking-wider">
                              Đã kích hoạt
                            </span>
                          ) : (
                            <span className="inline-flex items-center gap-0.5 px-2 py-0.5 bg-amber-50 text-amber-700 border border-amber-200 rounded text-[9px] font-black uppercase tracking-wider animate-pulse">
                              Chờ phê duyệt
                            </span>
                          )}
                        </td>
                        <td className="py-3 px-4 font-bold text-slate-600">
                          {adm.approvedBy ? (
                            <span className="text-emerald-700 font-black text-xs">{adm.approvedBy}</span>
                          ) : (
                            <span className="text-slate-400 font-mono italic text-[11px]">Chờ kích hoạt</span>
                          )}
                        </td>
                        <td className="py-3 px-4 text-right">
                          <div className="flex items-center justify-end gap-2">
                            {/* Approve Button */}
                            {!adm.approved && (
                              <button
                                onClick={() => handleApproveAdmin(adm.username)}
                                disabled={actionLoadingId !== null}
                                className="px-2.5 py-1 bg-emerald-600 hover:bg-emerald-700 text-white font-extrabold text-[10px] rounded-lg transition-all flex items-center gap-1 cursor-pointer disabled:opacity-50 shadow-sm hover:shadow active:scale-95"
                              >
                                {actionLoadingId === adm.username ? (
                                  <div className="w-3 h-3 border border-white/30 border-t-white rounded-full animate-spin" />
                                ) : (
                                  <ShieldCheck className="w-3.5 h-3.5" />
                                )}
                                Phê duyệt
                              </button>
                            )}

                            {/* Delete/Reject button */}
                            {!isDefaultAdmin && !isSelf && (
                              <button
                                onClick={() => handleDeleteAdmin(adm.username)}
                                disabled={actionLoadingId !== null}
                                className="px-2.5 py-1 bg-red-50 hover:bg-red-100 border border-red-200 text-red-600 font-black text-[10px] rounded-lg transition-all flex items-center gap-1 cursor-pointer disabled:opacity-50 active:scale-95"
                              >
                                {actionLoadingId === adm.username ? (
                                  <div className="w-3 h-3 border-2 border-red-350 border-t-red-700 rounded-full animate-spin" />
                                ) : (
                                  <Ban className="w-3.5 h-3.5" />
                                )}
                                {adm.approved ? 'Bỏ tài khoản' : 'Từ chối'}
                              </button>
                            )}
                          </div>
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
          )}

          {/* Quick guide on how approval security works */}
          <div className="bg-indigo-50/50 border border-indigo-100 rounded-xl p-3 flex items-start gap-2.5 shadow-inner">
            <HelpCircle className="w-4 h-4 text-indigo-600 shrink-0 mt-0.5 font-bold" />
            <p className="text-[11px] text-indigo-700 leading-relaxed font-semibold">
              <b>Giao thức phê duyệt:</b> Nhằm duy trì tuyệt đối tính toàn vẹn và ngăn chặn kể gian tự ý tạo tài khoản can thiệp mâm điều phối sản xuất, bất kỳ nhân sự đăng ký quản trị mới đều cần tài khoản quản trị viên tối cao (<span className="font-mono bg-white px-1 py-0.2 border rounded shadow-sm text-indigo-700 text-[10px]">admin</span>) hoặc tài khoản admin đang hoạt động cấp duyệt khác đăng nhập và kích hoạt trực tiếp từ bảng điều khiển này.
            </p>
          </div>
        </div>
      )}

    </div>
  );
};
