/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { createContext, useContext, useState, useCallback } from 'react';

// -----------------------------------------------------------------------
// Default labels for every position ID and section header ID
// -----------------------------------------------------------------------
export const DEFAULT_POSITION_LABELS: Record<string, string> = {
  // Section headers
  section_may_do:        'MÁY DÒ KIM LOẠI - MÂM RUNG',
  section_bang_tai_dai:  'NHÂN VIÊN CHỈNH BÁNH BĂNG TẢI DÀI (4 Vị Trí Trực)',
  section_mdg:           'CÁC TỔ THIẾT BỊ ĐÓNG GÓI (MĐG 1 - MĐG 7)',
  section_boc_tui:       'NHÂN VIÊN BÓC TÚI',
  section_can:           'CÂN KIỂM TRỌNG LƯỢNG',
  section_ep_mieng:      'TỔ NHÂN VIÊN ÉP MIỆNG (7 TRẠM NHIỆT_ÉP)',

  // Cell labels (shown inside each position card)
  do_kim_loai_mam_rung: 'Máy Dò - Mâm Rung',
  bang_tai_dai_1:       'Băng Tải Dài 1',
  bang_tai_dai_2:       'Băng Tải Dài 2',
  bang_tai_dai_3:       'Băng Tải Dài 3',
  bang_tai_dai_4:       'Băng Tải Dài 4',
  mdg_1:                'MĐG 1',
  mdg_2:                'MĐG 2',
  mdg_3:                'MĐG 3',
  mdg_4:                'MĐG 4',
  mdg_5:                'MĐG 5',
  mdg_6:                'MĐG 6',
  mdg_7:                'MĐG 7',
  boc_tui_1:            'Bóc Túi 1',
  boc_tui_2:            'Bóc Túi 2',
  boc_tui_3:            'Bóc Túi 3',
  can:                  'Cân Kiểm',
  ep_mieng_1:           'Ép Miệng 1',
  ep_mieng_2:           'Ép Miệng 2',
  ep_mieng_3:           'Ép Miệng 3',
  ep_mieng_4:           'Ép Miệng 4',
  ep_mieng_5:           'Ép Miệng 5',
  ep_mieng_6:           'Ép Miệng 6',
  ep_mieng_7:           'Ép Miệng 7',
};

const STORAGE_KEY = 'factory-position-labels';

// -----------------------------------------------------------------------
// Context type
// -----------------------------------------------------------------------
interface PositionLabelsContextType {
  labels: Record<string, string>;
  getLabel: (id: string) => string;
  updateLabel: (id: string, value: string) => void;
  updateLabels: (updates: Record<string, string>) => void;
  resetAll: () => void;
}

// -----------------------------------------------------------------------
// Context
// -----------------------------------------------------------------------
const PositionLabelsContext = createContext<PositionLabelsContextType | null>(null);

// -----------------------------------------------------------------------
// Provider
// -----------------------------------------------------------------------
export const PositionLabelsProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [labels, setLabels] = useState<Record<string, string>>(() => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY);
      if (saved) {
        return { ...DEFAULT_POSITION_LABELS, ...JSON.parse(saved) };
      }
    } catch { /* ignore */ }
    return { ...DEFAULT_POSITION_LABELS };
  });

  const persist = useCallback((next: Record<string, string>) => {
    // Only persist overrides that differ from defaults
    const overrides: Record<string, string> = {};
    for (const [k, v] of Object.entries(next)) {
      if (v !== DEFAULT_POSITION_LABELS[k]) overrides[k] = v;
    }
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(overrides));
    } catch { /* ignore */ }
  }, []);

  const getLabel = useCallback((id: string) => {
    return labels[id] ?? DEFAULT_POSITION_LABELS[id] ?? id;
  }, [labels]);

  const updateLabel = useCallback((id: string, value: string) => {
    setLabels(prev => {
      const next = { ...prev, [id]: value };
      persist(next);
      return next;
    });
  }, [persist]);

  const updateLabels = useCallback((updates: Record<string, string>) => {
    setLabels(prev => {
      const next = { ...prev, ...updates };
      persist(next);
      return next;
    });
  }, [persist]);

  const resetAll = useCallback(() => {
    try { localStorage.removeItem(STORAGE_KEY); } catch { /* ignore */ }
    setLabels({ ...DEFAULT_POSITION_LABELS });
  }, []);

  return (
    <PositionLabelsContext.Provider value={{ labels, getLabel, updateLabel, updateLabels, resetAll }}>
      {children}
    </PositionLabelsContext.Provider>
  );
};

// -----------------------------------------------------------------------
// Hook
// -----------------------------------------------------------------------
export const usePositionLabels = (): PositionLabelsContextType => {
  const ctx = useContext(PositionLabelsContext);
  if (!ctx) throw new Error('usePositionLabels must be used inside PositionLabelsProvider');
  return ctx;
};
