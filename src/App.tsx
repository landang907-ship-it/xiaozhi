/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import { useState, useEffect, useCallback } from 'react';
import { Layers, RefreshCw, Cpu, Activity, Clock, ShieldCheck, LayoutDashboard, CalendarDays } from 'lucide-react';
import { supabase } from './lib/supabase';
import { Machine } from './types';
import { MachineGrid } from './components/MachineGrid';
import { DataExtractor } from './components/DataExtractor';
import { AdminPanel } from './components/AdminPanel';
import { ShiftPlanner } from './components/ShiftPlanner';
import { PositionLabelsProvider } from './context/PositionLabelsContext';

export default function App() {
  const [machines, setMachines] = useState<Machine[]>([]);
  const [loading, setLoading] = useState(true);
  const [lastSync, setLastSync] = useState<Date>(new Date());
  
  // Tab controller state: 'dashboard' is Tab 1, 'extractor' is Tab 2
  const [activeTab, setActiveTab] = useState<'dashboard' | 'extractor' | 'admin'>('dashboard');
  // Sub-view inside Tab 1
  const [tab1View, setTab1View] = useState<'diagram' | 'planner'>('diagram');

  // Layout scale state for 7-inch tablets & customizable horizontal view scaling
  const [layoutScale, setLayoutScale] = useState<number>(() => {
    try {
      const saved = localStorage.getItem('factory-layout-scale');
      return saved ? Number(saved) : 0.85; // Default to 85% for high-density 7-inch landscape comfort
    } catch {
      return 0.85;
    }
  });

  // Tablet focused layout mode - Hides history, chatbot simulator and helper panels for beautiful absolute fit inside a 7-inch tablet screen
  const [tabletFocusMode, setTabletFocusMode] = useState<boolean>(() => {
    try {
      const saved = localStorage.getItem('factory-tablet-focus');
      return saved ? saved === 'true' : true; // Default to true so it immediately fits a 7-inch vertical landscape viewport perfectly!
    } catch {
      return true;
    }
  });

  const fetchMachines = useCallback(async () => {
    setLoading(true);
    const { data } = await supabase.from('machines').select();
    if (data) {
      setMachines(data as Machine[]);
      setLastSync(new Date());
    }
    setLoading(false);
  }, []);

  useEffect(() => {
    fetchMachines();

    // Setup real-time postgres changes subscription
    const channel = supabase
      .channel('machines-changes')
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'machines' },
        (payload) => {
          if (payload.eventType === 'UPDATE') {
            const updated = payload.new as Machine;
            setMachines((prev) => {
               // Update local list instantly
               return prev.map((m) => (m.id === updated.id ? updated : m));
            });
            setLastSync(new Date());
          }
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  }, [fetchMachines]);

  return (
    <PositionLabelsProvider>
    <div className="min-h-screen bg-slate-50 text-slate-800 flex flex-col selection:bg-indigo-500 selection:text-white print:bg-white print:text-black">
      
      {/* Top beautiful ambient glow bar */}
      <div className="h-[3px] bg-gradient-to-r from-indigo-500 via-blue-500 to-emerald-500 w-full shadow-lg print:hidden" />

      {/* Main Container */}
      <div className={`container mx-auto max-w-7xl flex-1 flex flex-col print:py-0 print:px-0 print:m-0 print:max-w-none ${tabletFocusMode ? 'py-2 px-2 gap-2.5 sm:py-3 sm:px-3 sm:gap-3' : 'py-6 px-4 md:px-6 gap-6'}`}>
        
        {/* Header bar */}
        <header className={`flex flex-col lg:flex-row lg:items-center justify-between gap-3 bg-white border border-slate-200 rounded-xl shadow-sm print:hidden ${tabletFocusMode ? 'p-3' : 'p-5'}`}>
          <div className="flex flex-col md:flex-row md:items-center gap-4 flex-1">
            <div className={`bg-indigo-50 text-indigo-600 rounded-xl border border-indigo-200 shadow-inner flex items-center justify-center shrink-0 w-fit ${tabletFocusMode ? 'p-2' : 'p-3'}`}>
              <Cpu className="w-6 h-6 animate-pulse" />
            </div>
            <div className="space-y-1 flex-1">
              <div className="flex flex-wrap items-center gap-3">
                <h1 className="text-xs md:text-sm font-black text-slate-900 tracking-wide uppercase flex items-center gap-1.5 shrink-0">
                  <span>Sơ đồ vận hành SENBEI</span>
                  <span className="text-[8px] font-bold text-emerald-700 bg-emerald-50 border border-emerald-200 px-1.5 py-0.5 rounded-md tracking-wider normal-case">
                    Real-time IIoT
                  </span>
                  <span className="text-[8px] text-slate-500 font-mono font-bold">V2.4</span>
                </h1>

                {/* Gọp Tablet View scale options seamlessly inside header block */}
                <div className="flex items-center gap-1 bg-slate-100 border border-slate-200 p-0.5 rounded-xl shrink-0">
                  <span className="text-[8px] font-bold text-slate-500 px-1.5 uppercase tracking-wider hidden sm:inline">Thu Phóng:</span>
                  {[0.65, 0.75, 0.85, 0.95, 1.0].map((scale) => {
                    const label = scale === 1.0 ? "Gốc" : scale === 0.65 ? "65%" : `${Math.round(scale * 100)}%`;
                    return (
                      <button
                        key={scale}
                        type="button"
                        onClick={() => {
                          setLayoutScale(scale);
                          localStorage.setItem('factory-layout-scale', String(scale));
                        }}
                        className={`px-2 py-0.5 text-[8px] font-black rounded-lg transition-all cursor-pointer ${
                          layoutScale === scale
                            ? 'bg-indigo-600 text-white shadow shadow-indigo-600/35'
                            : 'text-slate-600 hover:text-slate-900 hover:bg-slate-200'
                        }`}
                      >
                        {label}
                      </button>
                    );
                  })}
                  
                  <div className="w-px h-3.5 bg-slate-200 mx-1 shrink-0" />
                  
                  {/* Tablet Mode Toggle */}
                  <button
                    type="button"
                    onClick={() => {
                      const next = !tabletFocusMode;
                      setTabletFocusMode(next);
                      localStorage.setItem('factory-tablet-focus', String(next));
                    }}
                    className={`px-2 py-0.5 text-[8px] font-black rounded-lg transition-all flex items-center gap-1 cursor-pointer border ${
                      tabletFocusMode
                        ? 'bg-amber-400 hover:bg-amber-300 border-amber-300 text-slate-950 shadow-sm'
                        : 'bg-white border-slate-200 text-slate-600 hover:text-slate-905'
                    }`}
                  >
                    <span className={`w-1 h-1 rounded-full ${tabletFocusMode ? 'bg-slate-950 animate-pulse' : 'bg-slate-400'}`} />
                    7" Monitor
                  </button>
                </div>
              </div>
              
              {/* Integrated Status and Sync Info */}
              <div className="flex flex-wrap items-center gap-x-2.5 gap-y-0.5 text-[10px] text-slate-500 leading-snug">
                <div className="flex items-center gap-1.5 text-emerald-600 font-semibold">
                  <span className="relative flex h-1.5 w-1.5">
                    <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                    <span className="relative inline-flex rounded-full h-1.5 w-1.5 bg-emerald-500"></span>
                  </span>
                  <span>Hệ thống IIoT hoạt động lý tưởng, Supabase cập nhật siêu tốc.</span>
                </div>
                <span className="text-slate-300 hidden sm:inline">•</span>
                <div className="text-slate-500 font-semibold">
                  Đồng bộ cuối: <span className="text-emerald-600 font-mono font-bold">{lastSync.toLocaleTimeString('vi-VN')}</span>
                </div>
              </div>
            </div>
          </div>

          <div className="flex items-center justify-end gap-3 shrink-0">
            {/* Live UTC time visualizer */}
            <div className="flex items-center gap-1.5 px-3 py-1.5 bg-slate-100 rounded-xl border border-slate-200 text-xs text-slate-600">
              <Clock className="w-3.5 h-3.5 text-indigo-500" />
              <span className="font-mono">{new Date().toLocaleDateString('vi-VN')}</span>
            </div>

            <button
              onClick={fetchMachines}
              className="p-2.5 rounded-xl bg-white border border-slate-200 hover:bg-slate-55 text-slate-500 hover:text-slate-800 transition-all active:scale-95 cursor-pointer"
              title="Đồng bộ thủ công"
            >
              <RefreshCw className="w-4 h-4" />
            </button>
          </div>
        </header>

        {/* Dynamic Navigation Tab Bar */}
        <div className="flex bg-white p-1 border border-slate-200 rounded-2xl w-full sm:w-fit print:hidden z-10 self-start shadow-sm">
          <button
            type="button"
            onClick={() => setActiveTab('dashboard')}
            className={`flex-1 sm:flex-initial flex items-center justify-center gap-2 px-5 py-2.5 rounded-xl text-xs font-black uppercase tracking-wider transition-all cursor-pointer ${
              activeTab === 'dashboard'
                ? 'bg-indigo-600 text-white shadow'
                : 'text-slate-550 hover:text-slate-900 hover:bg-slate-100'
            }`}
          >
            <Activity className="w-4 h-4 text-indigo-550" />
            Tab 1 : Sơ đồ màn hình hiện tại
          </button>
          
          <button
            type="button"
            onClick={() => setActiveTab('extractor')}
            className={`flex-1 sm:flex-initial flex items-center justify-center gap-2 px-5 py-2.5 rounded-xl text-xs font-black uppercase tracking-wider transition-all cursor-pointer ${
              activeTab === 'extractor'
                ? 'bg-indigo-600 text-white shadow'
                : 'text-slate-550 hover:text-slate-900 hover:bg-slate-100'
            }`}
          >
            <Layers className="w-4 h-4 text-emerald-600 font-bold" />
            Tab 2 : Màn hình trích xuất dữ liệu
          </button>

          <button
            type="button"
            onClick={() => setActiveTab('admin')}
            className={`flex-1 sm:flex-initial flex items-center justify-center gap-2 px-5 py-2.5 rounded-xl text-xs font-black uppercase tracking-wider transition-all cursor-pointer ${
              activeTab === 'admin'
                ? 'bg-indigo-600 text-white shadow'
                : 'text-slate-550 hover:text-slate-900 hover:bg-slate-100'
            }`}
          >
            <ShieldCheck className="w-4 h-4 text-indigo-500" />
            ⚙ Quản Trị
          </button>
        </div>

        {/* Core dynamic content */}
        {loading && machines.length === 0 ? (
          <div className="flex-1 flex flex-col items-center justify-center py-24 space-y-4 print:hidden">
            <div className="w-10 h-10 border-4 border-slate-200 border-t-indigo-500 rounded-full animate-spin"></div>
            <p className="text-slate-500 text-sm font-medium">Đang tải trạng thái sơ đồ xưởng máy...</p>
          </div>
        ) : (
          <>
            {activeTab === 'dashboard' ? (
              <div className={`animate-fade-in flex-1 flex flex-col ${tabletFocusMode ? 'gap-3' : 'gap-4'}`}>

                {/* Sub-tab toggle for Sơ đồ / Lập kế hoạch */}
                <div className="flex bg-white p-1 border border-slate-200 rounded-2xl w-full sm:w-fit shadow-sm print:hidden self-start">
                  <button
                    type="button"
                    onClick={() => setTab1View('diagram')}
                    className={`flex items-center gap-1.5 px-4 py-2 rounded-xl text-xs font-black uppercase tracking-wider transition-all cursor-pointer ${
                      tab1View === 'diagram'
                        ? 'bg-indigo-600 text-white shadow'
                        : 'text-slate-550 hover:text-slate-900 hover:bg-slate-100'
                    }`}
                  >
                    <LayoutDashboard className="w-3.5 h-3.5" />
                    Sơ Đồ Nhà Xưởng
                  </button>
                  <button
                    type="button"
                    onClick={() => setTab1View('planner')}
                    className={`flex items-center gap-1.5 px-4 py-2 rounded-xl text-xs font-black uppercase tracking-wider transition-all cursor-pointer ${
                      tab1View === 'planner'
                        ? 'bg-violet-600 text-white shadow'
                        : 'text-slate-550 hover:text-slate-900 hover:bg-slate-100'
                    }`}
                  >
                    <CalendarDays className="w-3.5 h-3.5" />
                    Lập Kế Hoạch Sắp Ca
                  </button>
                </div>

                {tab1View === 'diagram' ? (
                  <div className="print:hidden">
                    <MachineGrid
                      machines={machines}
                      onRefresh={fetchMachines}
                      layoutScale={layoutScale}
                      tabletFocusMode={tabletFocusMode}
                    />
                  </div>
                ) : (
                  <div className="animate-fade-in">
                    <ShiftPlanner />
                  </div>
                )}

              </div>
            ) : activeTab === 'extractor' ? (
              <div className="animate-fade-in flex-1">
                <DataExtractor
                  machines={machines}
                  onRefreshMachines={fetchMachines}
                />
              </div>
            ) : (
              <div className="animate-fade-in flex-1">
                <AdminPanel />
              </div>
            )}
          </>
        )}



      </div>
    </div>
    </PositionLabelsProvider>
  );
}
