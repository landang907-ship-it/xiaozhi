/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState } from 'react';
import { 
  User, 
  UserX, 
  UserCheck, 
  X,
  Plus,
  ArrowDown
} from 'lucide-react';
import { Machine } from '../types';
import { usePositionLabels } from '../context/PositionLabelsContext';

interface MachineGridProps {
  machines: Machine[];
  onRefresh?: () => void;
  layoutScale: number;
  tabletFocusMode?: boolean;
}

export const MachineGrid: React.FC<MachineGridProps> = ({ machines, onRefresh, layoutScale, tabletFocusMode = false }) => {
  const { getLabel } = usePositionLabels();

  const scaleVal = (base: number) => {
    return Math.max(4, Math.round(base * layoutScale));
  };

  // Tablet Edit Modal State
  const [editingMachine, setEditingMachine] = useState<Machine | null>(null);
  const [inputOperator, setInputOperator] = useState('');
  const [selectedStatus, setSelectedStatus] = useState<Machine['status']>('EMPTY');
  const [isSaving, setIsSaving] = useState(false);

  // Find helper to ensure we pull current state of a node
  const getMachine = (id: string): Machine => {
    return machines.find(m => m.id === id) || {
      id,
      code: '',
      name: id,
      status: 'EMPTY',
      operator: '',
      updatedAt: new Date().toISOString()
    };
  };

  // Status configuration helper
  const getStatusColor = (status: Machine['status']) => {
    switch (status) {
      case 'PLAN':
        return 'bg-blue-600 border-blue-400 text-white shadow-md shadow-blue-500/20';
      case 'ALERT_ABSENT':
        return 'bg-red-650 border-red-500 text-white animate-pulse shadow-lg shadow-red-500/60 ring-2 ring-red-400';
      case 'REPLACED':
        return 'bg-amber-600 border-amber-400 text-white shadow-md shadow-amber-500/20';
      case 'EMPTY':
      default:
        return 'bg-slate-100 border-slate-200 text-slate-500 hover:bg-slate-205';
    }
  };

  const getStatusLabel = (status: Machine['status']) => {
    switch (status) {
      case 'PLAN': return 'PLAN (Có người đứng)';
      case 'ALERT_ABSENT': return 'VẤNG MẶT (Phát hiện)';
      case 'REPLACED': return 'REPLACED (Đã bổ sung)';
      case 'EMPTY':
      default: return 'Trống (Xám)';
    }
  };



  // Open the tablet edit drawer
  const openEditDialog = (mac: Machine) => {
    setEditingMachine(mac);
    setInputOperator(mac.operator || '');
    setSelectedStatus(mac.status);
  };

  // Save changes directly to our Express server
  const saveQuickUpdate = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!editingMachine) return;
    setIsSaving(true);
    try {
      const nextStatus = inputOperator.trim() === '' ? 'EMPTY' : selectedStatus;
      await fetch('/api/machines', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          id: editingMachine.id,
          status: nextStatus,
          operator: inputOperator.trim(),
          code: inputOperator.trim()
        })
      });
      setEditingMachine(null);
      if (onRefresh) onRefresh();
    } catch (err) {
      console.error(err);
    } finally {
      setIsSaving(false);
    }
  };

  // Helpers to render individual flow cells
  const renderCell = (id: string, customLabel?: string) => {
    const mac = getMachine(id);
    const colorClass = getStatusColor(mac.status);
    const minHeightVal = tabletFocusMode ? 64 * layoutScale : 82 * layoutScale;
    const paddingVal = tabletFocusMode ? Math.max(4, Math.round(6 * layoutScale)) : Math.max(4, Math.round(10 * layoutScale));
    
    return (
      <div
        onClick={() => openEditDialog(mac)}
        className={`relative cursor-pointer transition-all border-2 duration-200 rounded-xl text-center select-none active:scale-95 hover:brightness-110 flex flex-col justify-between ${colorClass}`}
        style={{ 
          minHeight: `${minHeightVal}px`, 
          padding: `${paddingVal}px` 
        }}
      >
        <div 
          className="uppercase font-bold opacity-60 tracking-wider truncate"
          style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px`, lineHeight: 1.1 }}
        >
          {customLabel || mac.name}
        </div>
        
        {/* Massive center code - Highlighted beautifully */}
        <div className={`flex items-center justify-center ${tabletFocusMode ? 'my-0.5' : 'my-1'}`}>
          {mac.operator ? (
            <div 
              className="font-mono font-black text-center bg-amber-400 text-slate-950 px-1.5 py-0.5 rounded shadow-sm border border-amber-300 tracking-wider truncate w-full"
              style={{ fontSize: `${scaleVal(tabletFocusMode ? 11 : 14)}px`, lineHeight: 1.1 }}
            >
              {mac.operator}
            </div>
          ) : (
            <div 
              className="font-black text-slate-400 tracking-wide truncate w-full"
              style={{ fontSize: `${scaleVal(tabletFocusMode ? 11 : 14)}px`, lineHeight: 1.1 }}
            >
              TRỐNG
            </div>
          )}
        </div>

        {/* Small indicator */}
        <div 
          className="opacity-45 font-semibold font-mono truncate"
          style={{ fontSize: `${scaleVal(tabletFocusMode ? 7 : 8)}px`, lineHeight: 1.1 }}
        >
          {mac.status === 'EMPTY' ? 'Chờ gán' : mac.status}
        </div>
      </div>
    );
  };

  // Custom styling for the Ép miệng downwards arrow shape cards
  const renderEpMiengArrowCell = (id: string, label: string) => {
    const mac = getMachine(id);
    const colorClass = getStatusColor(mac.status);
    const arrowHeightVal = tabletFocusMode ? 62 * layoutScale : 85 * layoutScale;
    const paddingValEp = tabletFocusMode ? Math.max(3, Math.round(4 * layoutScale)) : Math.max(4, Math.round(8 * layoutScale));

    return (
      <div 
        onClick={() => openEditDialog(mac)}
        className="flex flex-col items-center cursor-pointer group select-none active:scale-95 animate-fade-in"
      >
        {/* Operator box above arrow - Highlighted beautifully */}
        <div 
          className={`font-mono font-black truncate px-1.5 rounded transition-all text-center ${
            tabletFocusMode ? 'mb-0.5 shadow-sm' : 'mb-1 shadow-md'
          } ${
            mac.operator 
              ? 'bg-amber-400 text-slate-950 border border-amber-300 scale-105 saturate-125' 
              : 'text-gray-500/80 font-bold'
          }`}
          style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px` }}
        >
          {mac.operator ? mac.operator : 'TRỐNG'}
        </div>

        {/* Pentagonal Arrow body using CSS clip path */}
        <div 
          className={`w-full border-2 border-indigo-500/20 duration-200 text-center flex flex-col items-center justify-center transition-all group-hover:brightness-110 shadow-md ${colorClass}`}
          style={{
            clipPath: 'polygon(0% 0%, 100% 0%, 100% 70%, 50% 100%, 0% 70%)',
            height: `${arrowHeightVal}px`,
            padding: `${paddingValEp}px`
          }}
        >
          <div 
            className="font-extrabold text-white leading-none mb-0.5"
            style={{ fontSize: `${scaleVal(tabletFocusMode ? 14 : 18)}px` }}
          >
            {label}
          </div>
          <p 
            className="font-bold text-white/75 truncate tracking-tighter uppercase leading-none"
            style={{ fontSize: `${scaleVal(tabletFocusMode ? 6 : 7)}px` }}
          >
            {mac.status === 'EMPTY' ? 'ÉP MIỆNG' : mac.status}
          </p>
        </div>
      </div>
    );
  };

  return (
    <div id="factory-tablet-layout" className={tabletFocusMode ? "space-y-3" : "space-y-5"}>
      
      {/* DETAILED PRODUCTION LINE FLOW - DIRECTLY REPLICATING USER'S DRAWING */}
      <div 
        className="bg-white border border-slate-200 rounded-2xl shadow-sm relative overflow-hidden transition-all duration-300"
        style={{ padding: `${tabletFocusMode ? scaleVal(10) : scaleVal(20)}px` }}
      >
        
        {/* Subtle Ambient watermark background */}
        <div className="absolute right-6 top-6 opacity-[0.02] text-slate-900 select-none pointer-events-none">
          <span className="text-9xl font-black">FAB_IIOT</span>
        </div>

        <div className="flex flex-col lg:flex-row items-stretch" style={{ gap: `${tabletFocusMode ? scaleVal(10) : scaleVal(20)}px` }}>
          
          {/* LẮP ĐẶT TRỰC QUAN SIDEBAR: "SENBEI" BRAND COLUMN (EXACTLY MATCHING DECORATION) */}
          <div 
            className={`bg-slate-100 border border-slate-200 rounded-xl flex items-center justify-center shrink-0 shadow-inner ${
              tabletFocusMode ? 'py-4 px-2' : 'py-6 px-4'
            }`}
            style={{ width: `${tabletFocusMode ? scaleVal(50) : scaleVal(80)}px` }}
          >
            <h2 
              className="font-extrabold tracking-widest text-transparent bg-clip-text bg-gradient-to-br from-indigo-600 via-slate-700 to-indigo-900 uppercase text-center block leading-none lg:[writing-mode:vertical-lr]"
              style={{ fontSize: `${tabletFocusMode ? scaleVal(22) : scaleVal(34)}px` }}
            >
              SENBEI
            </h2>
          </div>

          {/* MAIN FLOW MATRIX LAYOUT */}
          <div className="flex-1 flex flex-col" style={{ gap: `${tabletFocusMode ? scaleVal(10) : scaleVal(20)}px` }}>
            
            {/* -------------------- ROW 1 (TOP) -------------------- */}
            <div className="grid grid-cols-1 md:grid-cols-3" style={{ gap: `${tabletFocusMode ? scaleVal(10) : scaleVal(20)}px` }}>
              
              {/* Box 1: Máy dò kim loại - Mâm Rung (Top Left) */}
              <div 
                className="bg-white border border-slate-200 rounded-xl flex flex-col justify-between shadow-sm" 
                style={{ padding: `${tabletFocusMode ? scaleVal(8) : scaleVal(14)}px` }}
              >
                <div>
                  <h3 
                    className="font-black uppercase text-indigo-600 tracking-wider mb-1.5 border-b border-slate-100 pb-1 animate-pulse" 
                    style={{ fontSize: `${scaleVal(tabletFocusMode ? 10 : 11)}px` }}
                  >
                    {getLabel('section_may_do')}
                  </h3>
                  <p 
                    className="text-slate-500 font-semibold mb-1.5" 
                    style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px` }}
                  >
                    Mâm tiếp nhận, kiểm soát dị vật
                  </p>
                </div>
                {renderCell('do_kim_loai_mam_rung', getLabel('do_kim_loai_mam_rung'))}
              </div>

              {/* Box 2: Băng Tải Dài chỉnh bánh (Top Right) */}
              <div 
                className="md:col-span-2 bg-white border border-slate-200 rounded-xl shadow-sm" 
                style={{ padding: `${tabletFocusMode ? scaleVal(8) : scaleVal(14)}px` }}
              >
                <h3 
                  className="font-black uppercase text-indigo-600 tracking-wider mb-1 border-b border-slate-100 pb-1" 
                  style={{ fontSize: `${scaleVal(tabletFocusMode ? 10 : 11)}px` }}
                >
                  {getLabel('section_bang_tai_dai')}
                </h3>
                <p 
                  className="text-slate-500 font-semibold mb-1.5" 
                  style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px` }}
                >
                  Chỉnh hướng bánh, xếp khay liên tục
                </p>
                
                <div className="grid grid-cols-2 sm:grid-cols-4" style={{ gap: `${tabletFocusMode ? scaleVal(8) : scaleVal(12)}px` }}>
                  {renderCell('bang_tai_dai_1', getLabel('bang_tai_dai_1'))}
                  {renderCell('bang_tai_dai_2', getLabel('bang_tai_dai_2'))}
                  {renderCell('bang_tai_dai_3', getLabel('bang_tai_dai_3'))}
                  {renderCell('bang_tai_dai_4', getLabel('bang_tai_dai_4'))}
                </div>
              </div>

            </div>

            {/* FLOW INDICATOR ARROWS */}
            <div className={`flex items-center justify-center text-indigo-500/20 ${tabletFocusMode ? 'my-0.5' : 'my-1'}`} style={{ gap: `${scaleVal(6)}px` }}>
              <div className="h-px bg-gradient-to-r from-transparent via-indigo-500/10 to-transparent flex-1" />
              <ArrowDown className="animate-bounce" style={{ width: `${scaleVal(tabletFocusMode ? 14 : 20)}px`, height: `${scaleVal(tabletFocusMode ? 14 : 20)}px` }} />
              <div className="h-px bg-gradient-to-r from-transparent via-indigo-500/10 to-transparent flex-1" />
            </div>

            {/* -------------------- ROW 2: PACKAGING MACHINES (MĐG 1 TO 7) -------------------- */}
            <div 
              className="bg-white border border-slate-200 rounded-xl text-left shadow-sm" 
              style={{ padding: `${tabletFocusMode ? scaleVal(8) : scaleVal(14)}px` }}
            >
              <h3 
                className="font-black uppercase text-indigo-600 tracking-wider mb-1 border-b border-slate-100 pb-1" 
                style={{ fontSize: `${scaleVal(tabletFocusMode ? 10 : 11)}px` }}
              >
                {getLabel('section_mdg')}
              </h3>
              <p 
                className="text-slate-500 font-semibold mb-1.5" 
                style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px` }}
              >
                Trạm đóng vỏ, nạp khí bảo quản Senbei
              </p>
              
              <div className="grid grid-cols-2 sm:grid-cols-4 lg:grid-cols-7" style={{ gap: `${tabletFocusMode ? scaleVal(8) : scaleVal(12)}px` }}>
                {renderCell('mdg_1', getLabel('mdg_1'))}
                {renderCell('mdg_2', getLabel('mdg_2'))}
                {renderCell('mdg_3', getLabel('mdg_3'))}
                {renderCell('mdg_4', getLabel('mdg_4'))}
                {renderCell('mdg_5', getLabel('mdg_5'))}
                {renderCell('mdg_6', getLabel('mdg_6'))}
                {renderCell('mdg_7', getLabel('mdg_7'))}
              </div>
            </div>

            {/* INTEGRATED FLOW INDICATOR ARROWS FOR EACH ROW */}
            <div className={`grid grid-cols-7 text-center text-indigo-500/20 pointer-events-none px-4 ${tabletFocusMode ? 'my-0.5' : 'my-1'}`} style={{ gap: `${tabletFocusMode ? scaleVal(8) : scaleVal(12)}px` }}>
              <div className="flex justify-center"><ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 12 : 16)}px`, height: `${scaleVal(tabletFocusMode ? 12 : 16)}px` }} /></div>
              <div className="flex justify-center"><ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 12 : 16)}px`, height: `${scaleVal(tabletFocusMode ? 12 : 16)}px` }} /></div>
              <div className="flex justify-center"><ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 12 : 16)}px`, height: `${scaleVal(tabletFocusMode ? 12 : 16)}px` }} /></div>
              <div className="flex justify-center"><ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 12 : 16)}px`, height: `${scaleVal(tabletFocusMode ? 12 : 16)}px` }} /></div>
              <div className="flex justify-center"><ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 12 : 16)}px`, height: `${scaleVal(tabletFocusMode ? 12 : 16)}px` }} /></div>
              <div className="flex justify-center"><ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 12 : 16)}px`, height: `${scaleVal(tabletFocusMode ? 12 : 16)}px` }} /></div>
              <div className="flex justify-center"><ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 12 : 16)}px`, height: `${scaleVal(tabletFocusMode ? 12 : 16)}px` }} /></div>
            </div>

            {/* -------------------- ROW 3: BAG PEELING (NHÂN VIÊN BÓC TÚI 1-3) -------------------- */}
            <div 
              className="bg-white border border-slate-200 rounded-xl grid grid-cols-1 md:grid-cols-4 items-center shadow-sm" 
              style={{ padding: `${tabletFocusMode ? scaleVal(8) : scaleVal(14)}px`, gap: `${tabletFocusMode ? scaleVal(10) : scaleVal(16)}px` }}
            >
              <div className="md:col-span-1 border-b md:border-b-0 pb-2 md:pb-0 border-slate-100">
                <h3 
                  className="font-black uppercase text-indigo-600 tracking-wider" 
                  style={{ fontSize: `${scaleVal(tabletFocusMode ? 10 : 11)}px` }}
                >
                  {getLabel('section_boc_tui')}
                </h3>
                <p 
                  className="text-slate-500 mt-1 leading-snug font-semibold" 
                  style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px` }}
                >
                  Rạch, bóc phôi túi xé hỗ trợ cấp liệu
                </p>
              </div>
              
              <div className="md:col-span-3 grid grid-cols-3" style={{ gap: `${tabletFocusMode ? scaleVal(8) : scaleVal(12)}px` }}>
                {renderCell('boc_tui_1', getLabel('boc_tui_1'))}
                {renderCell('boc_tui_2', getLabel('boc_tui_2'))}
                {renderCell('boc_tui_3', getLabel('boc_tui_3'))}
              </div>
            </div>

            {/* FLOW INDICATOR ARROWS */}
            <div className={`flex items-center justify-center text-indigo-500/20 ${tabletFocusMode ? 'my-0.5' : 'my-1'}`} style={{ gap: `${scaleVal(6)}px` }}>
              <div className="h-px bg-gradient-to-r from-transparent via-indigo-500/10 to-transparent flex-1" />
              <ArrowDown style={{ width: `${scaleVal(tabletFocusMode ? 14 : 20)}px`, height: `${scaleVal(tabletFocusMode ? 14 : 20)}px` }} />
              <div className="h-px bg-gradient-to-r from-transparent via-indigo-500/10 to-transparent flex-1" />
            </div>

            {/* -------------------- ROW 4: WEIGHING & SEALING (CÂN & ÉP MIỆNG) -------------------- */}
            <div className="grid grid-cols-1 lg:grid-cols-4" style={{ gap: `${tabletFocusMode ? scaleVal(10) : scaleVal(20)}px` }}>
              
              {/* Bottom Left: Cân (Styled as Circle Card exactly like picture) */}
              <div 
                className="bg-white border border-slate-200 rounded-xl flex flex-col justify-between items-center text-center shadow-sm" 
                style={{ padding: `${tabletFocusMode ? scaleVal(8) : scaleVal(14)}px` }}
              >
                <div className="w-full">
                  <h3 
                    className="font-black uppercase text-indigo-600 tracking-wider border-b border-slate-100 pb-1 mb-1" 
                    style={{ fontSize: `${scaleVal(tabletFocusMode ? 10 : 11)}px` }}
                  >
                    {getLabel('section_can')}
                  </h3>
                </div>
                
                {/* Circle Card representing "CÂN" */}
                {(() => {
                  const mac = getMachine('can');
                  const colorClass = getStatusColor(mac.status);
                  const circleSize = scaleVal(tabletFocusMode ? 102 : 125);
                  return (
                    <div 
                      onClick={() => openEditDialog(mac)}
                      className={`rounded-full cursor-pointer duration-200 border-2 select-none active:scale-95 flex flex-col items-center justify-center text-center transition-all ${colorClass}`}
                      style={{ 
                        width: `${circleSize}px`, 
                        height: `${circleSize}px`,
                        padding: `${tabletFocusMode ? scaleVal(8) : scaleVal(12)}px`
                      }}
                    >
                      <span className="font-extrabold tracking-widest text-slate-500 block mb-0.5" style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 10)}px` }}>CÂN</span>
                      <div className="w-full my-0.5 px-0.5">
                        {mac.operator ? (
                          <span 
                            className="font-mono font-black text-slate-950 bg-amber-400 border border-amber-300 px-1 py-0.5 rounded shadow-sm block truncate w-full"
                            style={{ fontSize: `${scaleVal(tabletFocusMode ? 11 : 13)}px` }}
                          >
                            {mac.operator}
                          </span>
                        ) : (
                          <span 
                            className="font-black text-slate-400 block truncate w-full"
                            style={{ fontSize: `${scaleVal(tabletFocusMode ? 11 : 14)}px` }}
                          >
                            TRỐNG
                          </span>
                        )}
                      </div>
                      <span className="font-mono opacity-50 block uppercase mt-0.5" style={{ fontSize: `${scaleVal(7)}px` }}>
                        {mac.status === 'EMPTY' ? 'Chở gán' : mac.status}
                      </span>
                    </div>
                  );
                })()}

                <span className="text-slate-550 font-semibold mt-1" style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px` }}>Đạt tiêu chí gram kiểm định</span>
              </div>

              {/* Bottom Right: Ép Miệng (7 Downward pentagonal arrows) */}
              <div 
                className="lg:col-span-3 bg-white border border-slate-200 rounded-xl shadow-sm text-left" 
                style={{ padding: `${tabletFocusMode ? scaleVal(8) : scaleVal(14)}px` }}
              >
                <h3 
                  className="font-black uppercase text-indigo-650 tracking-wider mb-1 border-b border-slate-100 pb-1" 
                  style={{ fontSize: `${scaleVal(tabletFocusMode ? 10 : 11)}px` }}
                >
                  {getLabel('section_ep_mieng')}
                </h3>
                <p 
                  className="text-slate-500 font-semibold mb-1.5" 
                  style={{ fontSize: `${scaleVal(tabletFocusMode ? 8 : 9)}px` }}
                >
                  Gia nhiệt hàn bao bì thành phẩm
                </p>
                
                <div className="grid grid-cols-4 sm:grid-cols-7" style={{ gap: `${tabletFocusMode ? scaleVal(6) : scaleVal(10)}px` }}>
                  {renderEpMiengArrowCell('ep_mieng_1', getLabel('ep_mieng_1'))}
                  {renderEpMiengArrowCell('ep_mieng_2', getLabel('ep_mieng_2'))}
                  {renderEpMiengArrowCell('ep_mieng_3', getLabel('ep_mieng_3'))}
                  {renderEpMiengArrowCell('ep_mieng_4', getLabel('ep_mieng_4'))}
                  {renderEpMiengArrowCell('ep_mieng_5', getLabel('ep_mieng_5'))}
                  {renderEpMiengArrowCell('ep_mieng_6', getLabel('ep_mieng_6'))}
                  {renderEpMiengArrowCell('ep_mieng_7', getLabel('ep_mieng_7'))}
                </div>
              </div>

            </div>

          </div>

        </div>

      </div>

      {/* QUICK TABLET EDIT DIALOG / POPUP SHEET (BETTER THAN UNSUCCESSFUL ALERTS) */}
      {editingMachine && (
        <div className="fixed inset-0 bg-slate-900/60 z-50 flex items-center justify-center p-4 backdrop-blur-sm animate-fade-in">
          <div className="bg-white border border-slate-200 rounded-2xl w-full max-w-md p-6 shadow-2xl relative overflow-hidden">
            
            {/* Close Button */}
            <button 
              onClick={() => setEditingMachine(null)}
              className="absolute right-4 top-4 text-slate-450 hover:text-slate-800 bg-slate-100 hover:bg-slate-200 p-1.5 rounded-lg transition-all"
            >
              <X className="w-4 h-4" />
            </button>

            {/* Modal Header */}
            <div>
              <span className="text-[9px] bg-indigo-50 text-indigo-600 font-extrabold border border-indigo-200 px-2.5 py-1 rounded-full uppercase tracking-wider block w-fit">
                Hệ Thống Trực Tiếp
              </span>
              <h3 className="text-lg font-black text-slate-900 mt-2 uppercase">
                Gán Nhân viên: {editingMachine.name}
              </h3>
              <p className="text-xs text-slate-500 mt-1">Đang trực tiếp sửa tại Tablet nhà xưởng</p>
            </div>

            {/* Modal Body form */}
            <form onSubmit={saveQuickUpdate} className="space-y-4 mt-5">
              
              {/* Operator code ID input */}
              <div className="space-y-1.5">
                <label className="text-xs font-bold text-slate-600 block uppercase tracking-wider">
                  Mã số nhân viên (MSNV)
                </label>
                <input
                  type="text"
                  required
                  placeholder="Ví dụ: VN002759, VN001..."
                  value={inputOperator}
                  onChange={(e) => setInputOperator(e.target.value)}
                  className="w-full bg-white border border-slate-350 rounded-xl px-4 py-2.5 text-sm text-slate-800 font-mono font-bold focus:outline-none focus:border-indigo-500 placeholder-slate-400"
                />
                <p className="text-[10px] text-slate-400">Mã định danh duy nhất của nhân viên vận hành.</p>
              </div>

              {/* Status Picker with nice colors */}
              <div className="space-y-2">
                <label className="text-xs font-bold text-slate-650 block uppercase tracking-wider">
                  Trạng thái gán máy
                </label>
                
                <div className="grid grid-cols-2 gap-2 text-xs">
                  <button
                    type="button"
                    onClick={() => setSelectedStatus('PLAN')}
                    className={`p-2.5 rounded-xl border text-left font-bold transition flex items-center gap-1.5 ${
                      selectedStatus === 'PLAN' 
                        ? 'bg-blue-600 border-blue-400 text-white' 
                        : 'bg-slate-50 border-slate-200 text-slate-600 hover:bg-slate-100'
                    }`}
                  >
                    <span className="w-2.5 h-2.5 rounded-full bg-blue-400 shadow shadow-blue-400" />
                    Có mặt (PLAN)
                  </button>

                  <button
                    type="button"
                    onClick={() => setSelectedStatus('ALERT_ABSENT')}
                    className={`p-2.5 rounded-xl border text-left font-bold transition flex items-center gap-1.5 ${
                      selectedStatus === 'ALERT_ABSENT' 
                        ? 'bg-red-650 border-red-500 text-white animate-pulse' 
                        : 'bg-slate-50 border-slate-200 text-slate-600 hover:bg-slate-100'
                    }`}
                  >
                    <span className="w-2.5 h-2.5 rounded-full bg-red-400 shadow shadow-red-400 animate-ping" />
                    Vắng mặt (ABSENT)
                  </button>

                  <button
                    type="button"
                    onClick={() => setSelectedStatus('REPLACED')}
                    className={`p-2.5 rounded-xl border text-left font-bold transition flex items-center gap-1.5 ${
                      selectedStatus === 'REPLACED' 
                        ? 'bg-amber-600 border-amber-400 text-white' 
                        : 'bg-slate-50 border-slate-200 text-slate-600 hover:bg-slate-100'
                    }`}
                  >
                    <span className="w-2.5 h-2.5 rounded-full bg-amber-400 shadow shadow-amber-400" />
                    Thay Thế (REPLACE)
                  </button>

                  <button
                    type="button"
                    onClick={() => {
                      setInputOperator('');
                      setSelectedStatus('EMPTY');
                    }}
                    className={`p-2.5 rounded-xl border text-left font-bold transition flex items-center gap-1.5 ${
                      selectedStatus === 'EMPTY' 
                        ? 'bg-slate-200 border-slate-355 text-slate-800' 
                        : 'bg-slate-50 border-slate-200 text-slate-600 hover:bg-slate-100'
                    }`}
                  >
                    <span className="w-2.5 h-2.5 rounded-full bg-slate-400" />
                    Bỏ Gán Trống
                  </button>
                </div>
              </div>

              {/* Submit panel buttons */}
              <div className="flex gap-2.5 pt-3 justify-end border-t border-slate-200">
                <button
                  type="button"
                  onClick={() => setEditingMachine(null)}
                  className="px-4 py-2 bg-slate-105 hover:bg-slate-205 text-slate-705 border border-slate-250 bg-slate-100 hover:bg-slate-200 text-slate-700 rounded-xl text-xs font-bold transition shadow-sm"
                >
                  Hủy bỏ
                </button>
                <button
                  type="submit"
                  disabled={isSaving}
                  className="px-5 py-2 bg-indigo-600 hover:bg-indigo-700 text-white font-bold rounded-xl text-xs flex items-center gap-1 active:scale-95 transition"
                >
                  {isSaving ? 'Đang lưu...' : 'Xác Nhận gán'}
                </button>
              </div>

            </form>
          </div>
        </div>
      )}



    </div>
  );
};
