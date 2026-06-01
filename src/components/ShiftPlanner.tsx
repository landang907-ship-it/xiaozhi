/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  Calendar,
  ChevronDown,
  Save,
  Zap,
  RefreshCw,
  Clock,
  Users,
  CheckCircle2,
  AlertCircle,
  X,
  Info,
  Download,
} from 'lucide-react';
import { usePositionLabels } from '../context/PositionLabelsContext';

// -----------------------------------------------------------------------
// Types
// -----------------------------------------------------------------------
interface PlanEntry {
  machineId: string;
  shift1?: string;
  shift2?: string;
  shift3?: string;
}

interface ShiftPlan {
  plannedDate: string;
  shiftType: '3_shifts' | '2_shifts';
  note: string;
  entries: PlanEntry[];
  updatedAt: string;
}

// -----------------------------------------------------------------------
// All 23 machine positions grouped by section
// -----------------------------------------------------------------------
const POSITION_GROUPS = [
  {
    sectionId: 'section_may_do',
    color: 'indigo',
    machines: [{ id: 'do_kim_loai_mam_rung', labelId: 'do_kim_loai_mam_rung' }],
  },
  {
    sectionId: 'section_bang_tai_dai',
    color: 'sky',
    machines: [
      { id: 'bang_tai_dai_1', labelId: 'bang_tai_dai_1' },
      { id: 'bang_tai_dai_2', labelId: 'bang_tai_dai_2' },
      { id: 'bang_tai_dai_3', labelId: 'bang_tai_dai_3' },
      { id: 'bang_tai_dai_4', labelId: 'bang_tai_dai_4' },
    ],
  },
  {
    sectionId: 'section_mdg',
    color: 'violet',
    machines: [
      { id: 'mdg_1', labelId: 'mdg_1' },
      { id: 'mdg_2', labelId: 'mdg_2' },
      { id: 'mdg_3', labelId: 'mdg_3' },
      { id: 'mdg_4', labelId: 'mdg_4' },
      { id: 'mdg_5', labelId: 'mdg_5' },
      { id: 'mdg_6', labelId: 'mdg_6' },
      { id: 'mdg_7', labelId: 'mdg_7' },
    ],
  },
  {
    sectionId: 'section_boc_tui',
    color: 'teal',
    machines: [
      { id: 'boc_tui_1', labelId: 'boc_tui_1' },
      { id: 'boc_tui_2', labelId: 'boc_tui_2' },
      { id: 'boc_tui_3', labelId: 'boc_tui_3' },
    ],
  },
  {
    sectionId: 'section_can',
    color: 'amber',
    machines: [{ id: 'can', labelId: 'can' }],
  },
  {
    sectionId: 'section_ep_mieng',
    color: 'rose',
    machines: [
      { id: 'ep_mieng_1', labelId: 'ep_mieng_1' },
      { id: 'ep_mieng_2', labelId: 'ep_mieng_2' },
      { id: 'ep_mieng_3', labelId: 'ep_mieng_3' },
      { id: 'ep_mieng_4', labelId: 'ep_mieng_4' },
      { id: 'ep_mieng_5', labelId: 'ep_mieng_5' },
      { id: 'ep_mieng_6', labelId: 'ep_mieng_6' },
      { id: 'ep_mieng_7', labelId: 'ep_mieng_7' },
    ],
  },
];

const SECTION_COLORS: Record<string, string> = {
  indigo: 'bg-indigo-50 border-indigo-200 text-indigo-700',
  sky: 'bg-sky-50 border-sky-200 text-sky-700',
  violet: 'bg-violet-50 border-violet-200 text-violet-700',
  teal: 'bg-teal-50 border-teal-200 text-teal-700',
  amber: 'bg-amber-50 border-amber-200 text-amber-700',
  rose: 'bg-rose-50 border-rose-200 text-rose-700',
};

const SECTION_DOT: Record<string, string> = {
  indigo: 'bg-indigo-500',
  sky: 'bg-sky-500',
  violet: 'bg-violet-500',
  teal: 'bg-teal-500',
  amber: 'bg-amber-500',
  rose: 'bg-rose-500',
};

// -----------------------------------------------------------------------
// Helper: get tomorrow date string YYYY-MM-DD
// -----------------------------------------------------------------------
function getTomorrowDateString(): string {
  const d = new Date();
  d.setDate(d.getDate() + 1);
  return d.toISOString().slice(0, 10);
}

// -----------------------------------------------------------------------
// Sub-component: Shift input cell
// -----------------------------------------------------------------------
interface ShiftInputProps {
  value: string;
  onChange: (v: string) => void;
  placeholder: string;
  shiftLabel: string;
  shiftColor: string;
}

const ShiftInput: React.FC<ShiftInputProps> = ({ value, onChange, placeholder, shiftLabel, shiftColor }) => (
  <div className="flex flex-col gap-0.5">
    <span className={`text-[9px] font-black uppercase tracking-wider px-1.5 py-0.5 rounded-md w-fit ${shiftColor}`}>
      {shiftLabel}
    </span>
    <div className="relative">
      <input
        type="text"
        value={value}
        onChange={e => onChange(e.target.value)}
        placeholder={placeholder}
        className="w-full bg-white border border-slate-200 rounded-lg px-2 py-1.5 text-[11px] font-mono font-bold text-slate-800 focus:outline-none focus:border-indigo-400 focus:ring-1 focus:ring-indigo-100 placeholder-slate-350 transition pr-6"
        style={{ minWidth: 0 }}
      />
      {value && (
        <button
          type="button"
          onClick={() => onChange('')}
          className="absolute right-1.5 top-1/2 -translate-y-1/2 text-slate-350 hover:text-red-400 transition"
        >
          <X className="w-3 h-3" />
        </button>
      )}
    </div>
  </div>
);

// -----------------------------------------------------------------------
// Main ShiftPlanner component
// -----------------------------------------------------------------------
export const ShiftPlanner: React.FC = () => {
  const { getLabel } = usePositionLabels();

  // Plan state
  const [plannedDate, setPlannedDate] = useState<string>(getTomorrowDateString());
  const [shiftType, setShiftType] = useState<'3_shifts' | '2_shifts'>('3_shifts');
  const [note, setNote] = useState<string>('');
  const [entries, setEntries] = useState<Record<string, PlanEntry>>({});

  // UI state
  const [isSaving, setIsSaving] = useState(false);
  const [saveStatus, setSaveStatus] = useState<'idle' | 'saved' | 'error'>('idle');
  const [isActivating, setIsActivating] = useState(false);
  const [activateShift, setActivateShift] = useState<'shift1' | 'shift2' | 'shift3'>('shift1');
  const [activateStatus, setActivateStatus] = useState<'idle' | 'done' | 'error'>('idle');
  const [isLoading, setIsLoading] = useState(true);
  const [isExportOpen, setIsExportOpen] = useState(false);

  // Load existing plan from server on mount
  const loadPlan = useCallback(async () => {
    setIsLoading(true);
    try {
      const res = await fetch('/api/plan');
      if (res.ok) {
        const plan: ShiftPlan = await res.json();
        setPlannedDate(plan.plannedDate);
        setShiftType(plan.shiftType || '3_shifts');
        setNote(plan.note || '');
        const entriesMap: Record<string, PlanEntry> = {};
        (plan.entries || []).forEach(e => {
          entriesMap[e.machineId] = e;
        });
        setEntries(entriesMap);
      }
    } catch (err) {
      console.error('Error loading plan:', err);
    } finally {
      setIsLoading(false);
    }
  }, []);

  useEffect(() => {
    loadPlan();
  }, [loadPlan]);

  // Update a specific shift field for a machine
  const updateEntry = useCallback((machineId: string, field: 'shift1' | 'shift2' | 'shift3', value: string) => {
    setEntries(prev => {
      const existing = prev[machineId] || { machineId };
      return {
        ...prev,
        [machineId]: { ...existing, [field]: value }
      };
    });
    setSaveStatus('idle');
    setActivateStatus('idle');
  }, []);

  // Save plan to server
  const savePlan = async () => {
    setIsSaving(true);
    setSaveStatus('idle');
    try {
      const entriesArray: PlanEntry[] = (Object.values(entries) as PlanEntry[]).filter(
        (e: PlanEntry) => e.shift1?.trim() || e.shift2?.trim() || e.shift3?.trim()
      );
      const res = await fetch('/api/plan', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ plannedDate, shiftType, note, entries: entriesArray })
      });
      if (res.ok) {
        setSaveStatus('saved');
        setTimeout(() => setSaveStatus('idle'), 3000);
      } else {
        setSaveStatus('error');
      }
    } catch {
      setSaveStatus('error');
    } finally {
      setIsSaving(false);
    }
  };

  // Activate a specific shift to the live diagram
  const activatePlan = async () => {
    setIsActivating(true);
    setActivateStatus('idle');
    try {
      // Save current state first, then activate
      const entriesArray: PlanEntry[] = (Object.values(entries) as PlanEntry[]).filter(
        (e: PlanEntry) => e.shift1?.trim() || e.shift2?.trim() || e.shift3?.trim()
      );
      await fetch('/api/plan', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ plannedDate, shiftType, note, entries: entriesArray })
      });

      const res = await fetch('/api/plan/activate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shift: activateShift })
      });
      if (res.ok) {
        setActivateStatus('done');
        setTimeout(() => setActivateStatus('idle'), 4000);
      } else {
        setActivateStatus('error');
      }
    } catch {
      setActivateStatus('error');
    } finally {
      setIsActivating(false);
    }
  };

  // Count filled positions per shift
  const countFilled = (field: 'shift1' | 'shift2' | 'shift3') =>
    Object.values(entries).filter(e => e[field]?.trim()).length;

  const totalPositions = POSITION_GROUPS.reduce((sum, g) => sum + g.machines.length, 0);

  const shiftLabels = shiftType === '3_shifts'
    ? [
        { key: 'shift1' as const, label: 'Ca 1', desc: '06:00 – 14:00', color: 'bg-sky-100 text-sky-700' },
        { key: 'shift2' as const, label: 'Ca 2', desc: '14:00 – 22:00', color: 'bg-violet-100 text-violet-700' },
        { key: 'shift3' as const, label: 'Ca 3', desc: '22:00 – 06:00', color: 'bg-slate-800 text-slate-100' },
      ]
    : [
        { key: 'shift1' as const, label: 'Ca Sáng', desc: '06:00 – 18:00', color: 'bg-amber-100 text-amber-800' },
        { key: 'shift2' as const, label: 'Ca Đêm',  desc: '18:00 – 06:00', color: 'bg-slate-800 text-slate-100' },
      ];

  const exportToExcel = () => {
    const headers = ['Nhóm vị trí', 'Mã vị trí', 'Tên vị trí'];
    shiftLabels.forEach(s => {
      headers.push(`${s.label} (${s.desc})`);
    });

    const rows = [headers];

    POSITION_GROUPS.forEach(group => {
      const groupName = getLabel(group.sectionId);
      group.machines.forEach(machine => {
        const entry = entries[machine.id] || {};
        const machineName = getLabel(machine.labelId);
        const row = [
          groupName,
          machine.id,
          machineName,
          entry.shift1 || '',
          entry.shift2 || '',
        ];
        if (shiftType === '3_shifts') {
          row.push(entry.shift3 || '');
        }
        rows.push(row);
      });
    });

    const csvContent = rows
      .map(row => row.map(val => `"${String(val).replace(/"/g, '""')}"`).join(','))
      .join('\n');

    const blob = new Blob(['\uFEFF' + csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.setAttribute('href', url);
    link.setAttribute('download', `Ke_hoach_sap_ca_${plannedDate}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    setIsExportOpen(false);
  };

  const exportToPDF = () => {
    const printWindow = window.open('', '_blank');
    if (!printWindow) return;

    const title = `KẾ HOẠCH SẮP CA - NGÀY ${plannedDate.split('-').reverse().join('/')}`;
    const shiftTypeText = shiftType === '3_shifts' ? 'Chế độ 3 ca (8 giờ)' : 'Chế độ 2 ca (12 giờ)';

    let rowsHtml = '';
    POSITION_GROUPS.forEach(group => {
      const groupName = getLabel(group.sectionId);
      group.machines.forEach(machine => {
        const entry = entries[machine.id] || {};
        const machineName = getLabel(machine.labelId);
        rowsHtml += `
          <tr>
            <td style="font-weight: bold;">${groupName}</td>
            <td style="font-family: monospace;">${machine.id}</td>
            <td>${machineName}</td>
            <td>${entry.shift1 || '-'}</td>
            <td>${entry.shift2 || '-'}</td>
            ${shiftType === '3_shifts' ? `<td>${entry.shift3 || '-'}</td>` : ''}
          </tr>
        `;
      });
    });

    const headersHtml = `
      <th>Tổ / Nhóm</th>
      <th>Mã vị trí</th>
      <th>Tên vị trí</th>
      ${shiftLabels.map(s => `<th>${s.label}<br><span style="font-size: 8px; font-weight: normal;">${s.desc}</span></th>`).join('')}
    `;

    const htmlContent = `
      <html>
        <head>
          <title>${title}</title>
          <style>
            body { font-family: system-ui, -apple-system, sans-serif; padding: 20px; color: #1e293b; }
            .header { text-align: center; margin-bottom: 20px; }
            .header h1 { margin: 0; font-size: 18px; text-transform: uppercase; color: #0f172a; }
            .header p { margin: 5px 0; font-size: 12px; color: #64748b; }
            .note-box { background: #f8fafc; border: 1px solid #e2e8f0; padding: 10px; border-radius: 6px; margin-bottom: 15px; font-size: 11px; }
            table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 11px; }
            th, td { border: 1px solid #cbd5e1; padding: 8px; text-align: left; }
            th { background-color: #f1f5f9; font-weight: bold; }
            tr:nth-child(even) { background-color: #f8fafc; }
            @media print {
              body { padding: 0; }
            }
          </style>
        </head>
        <body>
          <div class="header">
            <h1>${title}</h1>
            <p><strong>Loại ca:</strong> ${shiftTypeText} | <strong>Ngày tạo:</strong> ${new Date().toLocaleDateString('vi-VN')}</p>
          </div>
          ${note ? `<div class="note-box"><strong>Ghi chú:</strong> ${note}</div>` : ''}
          <table>
            <thead>
              <tr>${headersHtml}</tr>
            </thead>
            <tbody>
              ${rowsHtml}
            </tbody>
          </table>
          <script>
            window.onload = function() {
              window.print();
              setTimeout(function() { window.close(); }, 500);
            };
          </script>
        </body>
      </html>
    `;

    printWindow.document.write(htmlContent);
    printWindow.document.close();
    setIsExportOpen(false);
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-20">
        <div className="w-8 h-8 border-4 border-indigo-200 border-t-indigo-500 rounded-full animate-spin" />
        <span className="ml-3 text-slate-500 text-sm font-semibold">Đang tải kế hoạch ca...</span>
      </div>
    );
  }

  return (
    <div className="space-y-4">

      {/* ===== HEADER CONTROLS ===== */}
      <div className="bg-white border border-slate-200 rounded-2xl shadow-sm p-4">
        <div className="flex flex-wrap items-start gap-3">

          {/* Icon + Title */}
          <div className="flex items-center gap-2 shrink-0">
            <div className="bg-gradient-to-br from-indigo-500 to-violet-600 p-2 rounded-xl shadow-md shadow-indigo-500/25">
              <Calendar className="w-5 h-5 text-white" />
            </div>
            <div>
              <h2 className="font-black text-slate-900 text-sm uppercase tracking-wide leading-none">Lập Kế Hoạch Sắp Ca</h2>
              <p className="text-[10px] text-slate-500 font-semibold mt-0.5">Soạn thảo phân công nhân viên cho từng ca sản xuất</p>
            </div>
          </div>

          <div className="flex flex-wrap items-center gap-2 ml-auto">

            {/* Date picker */}
            <div className="flex flex-col gap-0.5">
              <label className="text-[9px] text-slate-500 font-bold uppercase tracking-wider px-0.5">Ngày lập kế hoạch</label>
              <div className="relative">
                <Calendar className="absolute left-2.5 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-indigo-500 pointer-events-none" />
                <input
                  type="date"
                  value={plannedDate}
                  onChange={e => { setPlannedDate(e.target.value); setSaveStatus('idle'); }}
                  className="pl-7 pr-3 py-1.5 bg-white border border-slate-200 rounded-xl text-xs font-bold text-slate-700 focus:outline-none focus:border-indigo-400 transition cursor-pointer"
                />
              </div>
            </div>

            {/* Shift type dropdown */}
            <div className="flex flex-col gap-0.5">
              <label className="text-[9px] text-slate-500 font-bold uppercase tracking-wider px-0.5">Loại ca</label>
              <div className="relative">
                <Clock className="absolute left-2.5 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-violet-500 pointer-events-none" />
                <select
                  value={shiftType}
                  onChange={e => { setShiftType(e.target.value as '3_shifts' | '2_shifts'); setSaveStatus('idle'); }}
                  className="pl-7 pr-8 py-1.5 bg-white border border-slate-200 rounded-xl text-xs font-bold text-slate-700 focus:outline-none focus:border-indigo-400 transition cursor-pointer appearance-none"
                >
                  <option value="3_shifts">3 Ca (8 giờ)</option>
                  <option value="2_shifts">2 Ca (12 giờ)</option>
                </select>
                <ChevronDown className="absolute right-2 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-slate-400 pointer-events-none" />
              </div>
            </div>

            {/* Reload button */}
            <div className="flex flex-col gap-0.5">
              <label className="text-[9px] text-transparent font-bold uppercase tracking-wider px-0.5">.</label>
              <button
                type="button"
                onClick={loadPlan}
                className="p-2 bg-slate-100 hover:bg-slate-200 border border-slate-200 rounded-xl transition text-slate-500 hover:text-slate-700"
                title="Tải lại kế hoạch từ máy chủ"
              >
                <RefreshCw className="w-3.5 h-3.5" />
              </button>
            </div>
          </div>
        </div>

        {/* Shift count summary badges */}
        <div className="flex flex-wrap gap-2 mt-3 pt-3 border-t border-slate-100">
          <div className="flex items-center gap-1.5 text-[10px] text-slate-500 font-semibold">
            <Users className="w-3.5 h-3.5 text-slate-400" />
            <span>Tổng vị trí: <strong className="text-slate-700">{totalPositions}</strong></span>
          </div>
          {shiftLabels.map(s => (
            <div key={s.key} className={`flex items-center gap-1 text-[10px] font-black px-2 py-0.5 rounded-full ${s.color}`}>
              <span>{s.label}:</span>
              <span>{countFilled(s.key)}/{totalPositions}</span>
            </div>
          ))}
          {note && (
            <div className="flex items-center gap-1 text-[10px] text-slate-500 font-semibold bg-slate-50 border border-slate-200 rounded-full px-2 py-0.5">
              <Info className="w-3 h-3" />
              <span className="truncate max-w-[180px]">{note}</span>
            </div>
          )}
        </div>
      </div>

      {/* ===== SHIFT LEGEND ===== */}
      <div className="flex flex-wrap gap-2">
        {shiftLabels.map(s => (
          <div key={s.key} className={`flex items-center gap-1.5 text-[10px] font-black px-3 py-1.5 rounded-xl border ${s.color} border-current border-opacity-20`}>
            <Clock className="w-3 h-3" />
            <span>{s.label}</span>
            <span className="opacity-60 font-semibold">({s.desc})</span>
          </div>
        ))}
        <div className="flex items-center gap-1.5 text-[10px] font-semibold text-slate-500 px-2 py-1.5">
          <Info className="w-3 h-3" />
          Nhập mã nhân viên (MSNV) vào từng ô ca tương ứng
        </div>
      </div>

      {/* ===== NOTE INPUT ===== */}
      <div className="bg-white border border-slate-200 rounded-xl px-4 py-2.5 flex items-center gap-3">
        <label className="text-[10px] font-black text-slate-500 uppercase tracking-wider shrink-0">Ghi chú:</label>
        <input
          type="text"
          value={note}
          onChange={e => { setNote(e.target.value); setSaveStatus('idle'); }}
          placeholder="Ghi chú thêm cho kế hoạch ca này..."
          className="flex-1 text-xs text-slate-700 font-semibold bg-transparent focus:outline-none placeholder-slate-350"
        />
      </div>

      {/* ===== POSITION GROUPS TABLE ===== */}
      <div className="space-y-3">
        {POSITION_GROUPS.map(group => (
          <div key={group.sectionId} className="bg-white border border-slate-200 rounded-2xl overflow-hidden shadow-sm">

            {/* Section header */}
            <div className={`flex items-center gap-2 px-4 py-2.5 border-b border-slate-100 ${SECTION_COLORS[group.color]}`}>
              <span className={`w-2 h-2 rounded-full shrink-0 ${SECTION_DOT[group.color]}`} />
              <span className="text-[10px] font-black uppercase tracking-wider">{getLabel(group.sectionId)}</span>
              <span className="ml-auto text-[9px] font-semibold opacity-70">{group.machines.length} vị trí</span>
            </div>

            {/* Machine rows */}
            <div className="divide-y divide-slate-50">
              {group.machines.map(machine => {
                const entry = entries[machine.id] || { machineId: machine.id };
                return (
                  <div key={machine.id} className="flex flex-col sm:flex-row sm:items-center gap-2 px-4 py-2.5 hover:bg-slate-50/60 transition">
                    {/* Machine label */}
                    <div className="sm:w-32 shrink-0">
                      <span className="text-[10px] font-black text-slate-700 uppercase tracking-wider">
                        {getLabel(machine.labelId)}
                      </span>
                      <code className="text-[8px] text-slate-400 block font-mono">{machine.id}</code>
                    </div>

                    {/* Shift inputs */}
                    <div className={`flex-1 grid gap-2 ${
                      shiftType === '3_shifts' ? 'grid-cols-1 sm:grid-cols-3' : 'grid-cols-1 sm:grid-cols-2'
                    }`}>
                      {shiftLabels.map(s => (
                        <ShiftInput
                          key={s.key}
                          value={entry[s.key] || ''}
                          onChange={v => updateEntry(machine.id, s.key, v)}
                          placeholder={`MSNV ${s.label}`}
                          shiftLabel={s.label}
                          shiftColor={s.color}
                        />
                      ))}
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
        ))}
      </div>

      {/* ===== FOOTER ACTION BAR ===== */}
      <div className="sticky bottom-0 bg-white/95 backdrop-blur-sm border border-slate-200 rounded-2xl shadow-lg px-5 py-3">
        <div className="flex flex-col sm:flex-row items-center gap-3">

          {/* Save button */}
          <button
            type="button"
            onClick={savePlan}
            disabled={isSaving}
            className={`flex items-center gap-2 px-5 py-2.5 rounded-xl text-xs font-black transition-all active:scale-95 ${
              saveStatus === 'saved'
                ? 'bg-emerald-500 text-white shadow shadow-emerald-500/30'
                : saveStatus === 'error'
                  ? 'bg-red-500 text-white'
                  : 'bg-indigo-600 hover:bg-indigo-700 text-white shadow shadow-indigo-500/30'
            }`}
          >
            {isSaving ? (
              <div className="w-3.5 h-3.5 border-2 border-white/40 border-t-white rounded-full animate-spin" />
            ) : saveStatus === 'saved' ? (
              <CheckCircle2 className="w-3.5 h-3.5" />
            ) : saveStatus === 'error' ? (
              <AlertCircle className="w-3.5 h-3.5" />
            ) : (
              <Save className="w-3.5 h-3.5" />
            )}
            {isSaving ? 'Đang lưu...' : saveStatus === 'saved' ? 'Đã lưu kế hoạch!' : saveStatus === 'error' ? 'Lỗi lưu!' : 'Lưu Kế Hoạch'}
          </button>

          <div className="w-px h-6 bg-slate-200 hidden sm:block" />

          {/* Activate section */}
          <div className="flex flex-wrap items-center gap-2 flex-1">
            <div className="flex items-center gap-1.5 text-[10px] font-black text-slate-600 uppercase tracking-wider">
              <Zap className="w-3.5 h-3.5 text-amber-500" />
              Kích hoạt ca lên sơ đồ:
            </div>

            {/* Shift selector dropdown */}
            <div className="relative">
              <select
                value={activateShift}
                onChange={e => setActivateShift(e.target.value as 'shift1' | 'shift2' | 'shift3')}
                className="pl-3 pr-8 py-2 bg-white border border-slate-200 rounded-xl text-[11px] font-black text-slate-700 focus:outline-none focus:border-amber-400 transition cursor-pointer appearance-none"
              >
                {shiftLabels.map(s => (
                  <option key={s.key} value={s.key}>
                    {s.label} ({s.desc})
                  </option>
                ))}
              </select>
              <ChevronDown className="absolute right-2 top-1/2 -translate-y-1/2 w-3.5 h-3.5 text-slate-400 pointer-events-none" />
            </div>

            {/* Activate button */}
            <button
              type="button"
              onClick={activatePlan}
              disabled={isActivating}
              className={`flex items-center gap-2 px-5 py-2.5 rounded-xl text-xs font-black transition-all active:scale-95 ${
                activateStatus === 'done'
                  ? 'bg-emerald-500 text-white shadow shadow-emerald-500/30'
                  : activateStatus === 'error'
                    ? 'bg-red-500 text-white'
                    : 'bg-gradient-to-r from-amber-500 to-orange-500 hover:from-amber-600 hover:to-orange-600 text-white shadow shadow-amber-500/30'
              }`}
            >
              {isActivating ? (
                <div className="w-3.5 h-3.5 border-2 border-white/40 border-t-white rounded-full animate-spin" />
              ) : activateStatus === 'done' ? (
                <CheckCircle2 className="w-3.5 h-3.5" />
              ) : activateStatus === 'error' ? (
                <AlertCircle className="w-3.5 h-3.5" />
              ) : (
                <Zap className="w-3.5 h-3.5" />
              )}
              {isActivating ? 'Đang kích hoạt...' : activateStatus === 'done' ? 'Đã kích hoạt!' : activateStatus === 'error' ? 'Lỗi kích hoạt!' : 'Kích Hoạt Ngay'}
            </button>

            {/* Export dropdown */}
            <div className="relative">
              <button
                type="button"
                onClick={() => setIsExportOpen(v => !v)}
                className="flex items-center gap-1.5 px-4 py-2.5 bg-slate-100 hover:bg-slate-200 border border-slate-200 rounded-xl text-xs font-black text-slate-700 transition-all active:scale-95 shadow-sm"
              >
                <Download className="w-3.5 h-3.5" />
                Xuất lịch
                <ChevronDown className="w-3 h-3 text-slate-400" />
              </button>

              {isExportOpen && (
                <>
                  {/* Overlay to close dropdown on click outside */}
                  <div className="fixed inset-0 z-40" onClick={() => setIsExportOpen(false)} />
                  <div className="absolute bottom-full left-0 mb-2 w-32 bg-white border border-slate-200 rounded-xl shadow-xl z-50 overflow-hidden divide-y divide-slate-100 animate-in fade-in slide-in-from-bottom-2 duration-150">
                    <button
                      type="button"
                      onClick={exportToExcel}
                      className="w-full text-left px-3.5 py-2 text-[11px] font-black text-slate-700 hover:bg-indigo-50 hover:text-indigo-600 transition"
                    >
                      1. Excel (.csv)
                    </button>
                    <button
                      type="button"
                      onClick={exportToPDF}
                      className="w-full text-left px-3.5 py-2 text-[11px] font-black text-slate-700 hover:bg-indigo-50 hover:text-indigo-600 transition"
                    >
                      2. PDF (In lịch)
                    </button>
                  </div>
                </>
              )}
            </div>

            {activateStatus === 'done' && (
              <span className="text-[10px] text-emerald-600 font-bold animate-pulse flex items-center gap-1">
                <CheckCircle2 className="w-3 h-3" />
                Sơ đồ đã cập nhật theo ca vừa chọn!
              </span>
            )}
          </div>
        </div>
      </div>

    </div>
  );
};
