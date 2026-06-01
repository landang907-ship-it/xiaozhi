/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useRef, useEffect } from 'react';
import {
  Lock, Unlock, LogOut, RotateCcw, Save, Eye, EyeOff,
  ShieldCheck, Settings2, ChevronDown, ChevronUp, Check, AlertTriangle
} from 'lucide-react';
import { usePositionLabels, DEFAULT_POSITION_LABELS } from '../context/PositionLabelsContext';

// -----------------------------------------------------------------------
// Admin password (simple client-side gate)
// -----------------------------------------------------------------------
const ADMIN_PASSWORD = 'admin123';

// -----------------------------------------------------------------------
// Groups for the editor table
// -----------------------------------------------------------------------
interface LabelGroup {
  sectionId: string;
  sectionLabel: string;
  items: { id: string; defaultName: string }[];
}

const LABEL_GROUPS: LabelGroup[] = [
  {
    sectionId: 'section_may_do',
    sectionLabel: 'Máy Dò Kim Loại – Mâm Rung',
    items: [{ id: 'do_kim_loai_mam_rung', defaultName: 'Máy Dò - Mâm Rung' }],
  },
  {
    sectionId: 'section_bang_tai_dai',
    sectionLabel: 'Băng Tải Dài (Chỉnh Bánh)',
    items: [
      { id: 'bang_tai_dai_1', defaultName: 'Băng Tải Dài 1' },
      { id: 'bang_tai_dai_2', defaultName: 'Băng Tải Dài 2' },
      { id: 'bang_tai_dai_3', defaultName: 'Băng Tải Dài 3' },
      { id: 'bang_tai_dai_4', defaultName: 'Băng Tải Dài 4' },
    ],
  },
  {
    sectionId: 'section_mdg',
    sectionLabel: 'Tổ Thiết Bị Đóng Gói (MĐG)',
    items: [
      { id: 'mdg_1', defaultName: 'MĐG 1' },
      { id: 'mdg_2', defaultName: 'MĐG 2' },
      { id: 'mdg_3', defaultName: 'MĐG 3' },
      { id: 'mdg_4', defaultName: 'MĐG 4' },
      { id: 'mdg_5', defaultName: 'MĐG 5' },
      { id: 'mdg_6', defaultName: 'MĐG 6' },
      { id: 'mdg_7', defaultName: 'MĐG 7' },
    ],
  },
  {
    sectionId: 'section_boc_tui',
    sectionLabel: 'Nhân Viên Bóc Túi',
    items: [
      { id: 'boc_tui_1', defaultName: 'Bóc Túi 1' },
      { id: 'boc_tui_2', defaultName: 'Bóc Túi 2' },
      { id: 'boc_tui_3', defaultName: 'Bóc Túi 3' },
    ],
  },
  {
    sectionId: 'section_can',
    sectionLabel: 'Cân Kiểm Trọng Lượng',
    items: [{ id: 'can', defaultName: 'Cân Kiểm' }],
  },
  {
    sectionId: 'section_ep_mieng',
    sectionLabel: 'Tổ Nhân Viên Ép Miệng',
    items: [
      { id: 'ep_mieng_1', defaultName: 'Ép Miệng 1' },
      { id: 'ep_mieng_2', defaultName: 'Ép Miệng 2' },
      { id: 'ep_mieng_3', defaultName: 'Ép Miệng 3' },
      { id: 'ep_mieng_4', defaultName: 'Ép Miệng 4' },
      { id: 'ep_mieng_5', defaultName: 'Ép Miệng 5' },
      { id: 'ep_mieng_6', defaultName: 'Ép Miệng 6' },
      { id: 'ep_mieng_7', defaultName: 'Ép Miệng 7' },
    ],
  },
];

// -----------------------------------------------------------------------
// Login screen
// -----------------------------------------------------------------------
const LoginScreen: React.FC<{ onLogin: () => void }> = ({ onLogin }) => {
  const [pw, setPw] = useState('');
  const [showPw, setShowPw] = useState(false);
  const [error, setError] = useState('');
  const [shaking, setShaking] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => { inputRef.current?.focus(); }, []);

  const submit = (e: React.FormEvent) => {
    e.preventDefault();
    if (pw === ADMIN_PASSWORD) {
      onLogin();
    } else {
      setError('Mật khẩu không đúng. Vui lòng thử lại.');
      setShaking(true);
      setPw('');
      setTimeout(() => setShaking(false), 500);
      inputRef.current?.focus();
    }
  };

  return (
    <div className="flex flex-col items-center justify-center min-h-[60vh] px-4">
      {/* Glassmorphism card */}
      <div
        className={`relative w-full max-w-sm bg-white border border-slate-200 rounded-2xl shadow-xl overflow-hidden transition-all ${shaking ? 'animate-shake' : ''}`}
        style={{ animationDuration: '0.4s' }}
      >
        {/* Top accent bar */}
        <div className="h-1 w-full bg-gradient-to-r from-indigo-500 via-blue-500 to-emerald-400" />

        <div className="p-8">
          {/* Icon */}
          <div className="flex justify-center mb-5">
            <div className="bg-indigo-50 border border-indigo-200 rounded-2xl p-4 shadow-inner">
              <Lock className="w-8 h-8 text-indigo-600" />
            </div>
          </div>

          <h2 className="text-center text-xl font-black text-slate-900 mb-1">Khu vực quản trị</h2>
          <p className="text-center text-xs text-slate-500 mb-6 font-semibold">
            Chỉ dành cho quản lý nhà xưởng SENBEI
          </p>

          <form onSubmit={submit} className="space-y-4">
            <div className="space-y-1.5">
              <label className="text-xs font-bold text-slate-600 uppercase tracking-wider block">
                Mật khẩu Admin
              </label>
              <div className="relative">
                <input
                  ref={inputRef}
                  type={showPw ? 'text' : 'password'}
                  value={pw}
                  onChange={e => { setPw(e.target.value); setError(''); }}
                  placeholder="Nhập mật khẩu..."
                  className="w-full bg-slate-50 border border-slate-200 rounded-xl px-4 py-2.5 pr-10 text-sm text-slate-800 font-mono font-bold focus:outline-none focus:border-indigo-400 focus:bg-white transition placeholder-slate-400"
                />
                <button
                  type="button"
                  tabIndex={-1}
                  onClick={() => setShowPw(v => !v)}
                  className="absolute right-3 top-1/2 -translate-y-1/2 text-slate-400 hover:text-slate-700 transition"
                >
                  {showPw ? <EyeOff className="w-4 h-4" /> : <Eye className="w-4 h-4" />}
                </button>
              </div>
              {error && (
                <p className="text-xs text-red-500 font-bold flex items-center gap-1">
                  <AlertTriangle className="w-3 h-3" /> {error}
                </p>
              )}
            </div>

            <button
              type="submit"
              className="w-full bg-indigo-600 hover:bg-indigo-700 active:scale-95 text-white font-black rounded-xl py-2.5 text-sm transition-all flex items-center justify-center gap-2 shadow"
            >
              <Unlock className="w-4 h-4" />
              Đăng Nhập
            </button>
          </form>

          <p className="text-center text-[10px] text-slate-400 font-semibold mt-5">
            Liên hệ IT nếu quên mật khẩu · SENBEI IIoT v2.4
          </p>
        </div>
      </div>
    </div>
  );
};

// -----------------------------------------------------------------------
// Section row in editor
// -----------------------------------------------------------------------
interface SectionEditorProps {
  group: LabelGroup;
  draft: Record<string, string>;
  onChange: (id: string, val: string) => void;
}

const SectionEditor: React.FC<SectionEditorProps> = ({ group, draft, onChange }) => {
  const [open, setOpen] = useState(true);

  const hasChanges = [group.sectionId, ...group.items.map(i => i.id)].some(
    id => draft[id] !== DEFAULT_POSITION_LABELS[id]
  );

  return (
    <div className="border border-slate-200 rounded-xl overflow-hidden">
      {/* Section header row */}
      <button
        type="button"
        onClick={() => setOpen(v => !v)}
        className="w-full flex items-center justify-between px-4 py-3 bg-slate-50 hover:bg-slate-100 transition text-left"
      >
        <div className="flex items-center gap-2">
          <Settings2 className="w-4 h-4 text-indigo-500 shrink-0" />
          <span className="font-black text-xs text-slate-800 uppercase tracking-wider">
            {group.sectionLabel}
          </span>
          {hasChanges && (
            <span className="text-[9px] bg-amber-100 text-amber-700 border border-amber-200 font-bold px-1.5 py-0.5 rounded-full">
              Đã sửa
            </span>
          )}
        </div>
        {open ? <ChevronUp className="w-4 h-4 text-slate-400" /> : <ChevronDown className="w-4 h-4 text-slate-400" />}
      </button>

      {open && (
        <div className="divide-y divide-slate-100">
          {/* Section header label */}
          <div className="px-4 py-2.5 bg-indigo-50/40 flex flex-col sm:flex-row sm:items-center gap-1.5">
            <div className="sm:w-48 shrink-0">
              <span className="text-[10px] font-bold text-indigo-600 uppercase tracking-wider">Tiêu đề nhóm</span>
            </div>
            <input
              type="text"
              value={draft[group.sectionId] ?? DEFAULT_POSITION_LABELS[group.sectionId]}
              onChange={e => onChange(group.sectionId, e.target.value)}
              className="flex-1 bg-white border border-indigo-200 rounded-lg px-3 py-1.5 text-xs font-bold text-slate-700 focus:outline-none focus:border-indigo-500 transition"
            />
            {draft[group.sectionId] !== DEFAULT_POSITION_LABELS[group.sectionId] && (
              <button
                type="button"
                onClick={() => onChange(group.sectionId, DEFAULT_POSITION_LABELS[group.sectionId])}
                className="shrink-0 text-[10px] text-slate-400 hover:text-red-500 font-bold transition"
                title="Về mặc định"
              >
                ↺
              </button>
            )}
          </div>

          {/* Individual cell labels */}
          {group.items.map(item => {
            const changed = draft[item.id] !== DEFAULT_POSITION_LABELS[item.id];
            return (
              <div key={item.id} className="px-4 py-2.5 flex flex-col sm:flex-row sm:items-center gap-1.5 hover:bg-slate-50/60 transition">
                <div className="sm:w-48 shrink-0">
                  <code className="text-[9px] text-slate-400 font-mono block">{item.id}</code>
                  <span className="text-[10px] text-slate-500 font-semibold">Mặc định: {item.defaultName}</span>
                </div>
                <input
                  type="text"
                  value={draft[item.id] ?? item.defaultName}
                  onChange={e => onChange(item.id, e.target.value)}
                  className={`flex-1 bg-white border rounded-lg px-3 py-1.5 text-xs font-bold text-slate-800 focus:outline-none transition ${
                    changed
                      ? 'border-amber-300 focus:border-amber-500 bg-amber-50/30'
                      : 'border-slate-200 focus:border-indigo-400'
                  }`}
                />
                {changed && (
                  <button
                    type="button"
                    onClick={() => onChange(item.id, DEFAULT_POSITION_LABELS[item.id])}
                    className="shrink-0 text-[10px] text-slate-400 hover:text-red-500 font-bold transition"
                    title="Về mặc định"
                  >
                    ↺
                  </button>
                )}
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
};

// -----------------------------------------------------------------------
// Main AdminPanel component
// -----------------------------------------------------------------------
export const AdminPanel: React.FC = () => {
  const { labels, updateLabels, resetAll } = usePositionLabels();
  const [isLoggedIn, setIsLoggedIn] = useState(false);

  // Local draft state – only applied when user clicks "Save"
  const [draft, setDraft] = useState<Record<string, string>>({});
  const [saved, setSaved] = useState(false);
  const [confirmReset, setConfirmReset] = useState(false);

  // Initialise draft from current labels when logging in
  const handleLogin = () => {
    setDraft({ ...labels });
    setIsLoggedIn(true);
  };

  const handleLogout = () => {
    setIsLoggedIn(false);
    setDraft({});
    setSaved(false);
    setConfirmReset(false);
  };

  const handleChange = (id: string, val: string) => {
    setDraft(prev => ({ ...prev, [id]: val }));
    setSaved(false);
  };

  const handleSave = () => {
    updateLabels(draft);
    setSaved(true);
    setTimeout(() => setSaved(false), 2500);
  };

  const handleReset = () => {
    if (!confirmReset) { setConfirmReset(true); return; }
    resetAll();
    setDraft({ ...DEFAULT_POSITION_LABELS });
    setConfirmReset(false);
    setSaved(false);
  };

  // Count pending changes
  const changedCount = Object.entries(draft).filter(
    ([k, v]) => v !== DEFAULT_POSITION_LABELS[k]
  ).length;

  if (!isLoggedIn) {
    return <LoginScreen onLogin={handleLogin} />;
  }

  return (
    <div className="max-w-3xl mx-auto space-y-5 pb-10">
      {/* Header */}
      <div className="flex items-center justify-between bg-white border border-slate-200 rounded-2xl shadow-sm px-5 py-4">
        <div className="flex items-center gap-3">
          <div className="bg-indigo-50 border border-indigo-200 rounded-xl p-2.5 shadow-inner">
            <ShieldCheck className="w-5 h-5 text-indigo-600" />
          </div>
          <div>
            <h2 className="font-black text-slate-900 text-sm uppercase tracking-wide">Quản trị nhà xưởng</h2>
            <p className="text-[10px] text-slate-500 font-semibold">Chỉnh sửa tên hiển thị các vị trí trên sơ đồ</p>
          </div>
        </div>

        <button
          onClick={handleLogout}
          className="flex items-center gap-1.5 text-xs font-bold text-slate-500 hover:text-red-600 border border-slate-200 hover:border-red-200 hover:bg-red-50 px-3 py-1.5 rounded-xl transition-all"
        >
          <LogOut className="w-3.5 h-3.5" />
          Đăng xuất
        </button>
      </div>

      {/* Info banner */}
      <div className="bg-indigo-50 border border-indigo-200 rounded-xl px-4 py-3 text-xs text-indigo-700 font-semibold flex items-center gap-2">
        <Settings2 className="w-4 h-4 shrink-0" />
        Thay đổi tên hiển thị bên dưới rồi nhấn <strong>"Lưu thay đổi"</strong>. Tên mới sẽ được áp dụng ngay trên Tab 1 (sơ đồ).
      </div>

      {/* Editor groups */}
      <div className="space-y-3">
        {LABEL_GROUPS.map(group => (
          <SectionEditor
            key={group.sectionId}
            group={group}
            draft={draft}
            onChange={handleChange}
          />
        ))}
      </div>

      {/* Action bar */}
      <div className="sticky bottom-0 bg-white/95 backdrop-blur border border-slate-200 rounded-2xl shadow-lg px-5 py-3 flex flex-col sm:flex-row items-center justify-between gap-3">
        <div className="text-xs text-slate-500 font-semibold">
          {changedCount > 0 ? (
            <span className="text-amber-600 font-black">⚠ {changedCount} thay đổi chưa lưu</span>
          ) : (
            <span className="text-slate-400">Chưa có thay đổi nào</span>
          )}
        </div>

        <div className="flex items-center gap-2">
          {/* Reset all */}
          <button
            type="button"
            onClick={handleReset}
            onBlur={() => setTimeout(() => setConfirmReset(false), 1500)}
            className={`flex items-center gap-1.5 text-xs font-bold px-3 py-2 rounded-xl border transition-all ${
              confirmReset
                ? 'bg-red-500 border-red-400 text-white hover:bg-red-600'
                : 'text-slate-500 border-slate-200 hover:border-red-200 hover:text-red-600 hover:bg-red-50'
            }`}
          >
            <RotateCcw className="w-3.5 h-3.5" />
            {confirmReset ? 'Xác nhận reset?' : 'Reset mặc định'}
          </button>

          {/* Save */}
          <button
            type="button"
            onClick={handleSave}
            disabled={changedCount === 0}
            className={`flex items-center gap-1.5 text-xs font-bold px-4 py-2 rounded-xl transition-all active:scale-95 ${
              saved
                ? 'bg-emerald-500 border-emerald-400 text-white border'
                : changedCount > 0
                  ? 'bg-indigo-600 hover:bg-indigo-700 text-white shadow'
                  : 'bg-slate-100 text-slate-400 border border-slate-200 cursor-not-allowed'
            }`}
          >
            {saved ? <Check className="w-3.5 h-3.5" /> : <Save className="w-3.5 h-3.5" />}
            {saved ? 'Đã lưu!' : 'Lưu thay đổi'}
          </button>
        </div>
      </div>
    </div>
  );
};
