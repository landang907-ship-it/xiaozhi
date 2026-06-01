/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

export interface Machine {
  id: string;
  code: string;         // e.g., VN002759
  name: string;         // "Mâm Rung", "Máy Dò Kim Loại", "Băng Tải Dài", "Máy ĐG1"
  status: 'EMPTY' | 'PLAN' | 'ALERT_ABSENT' | 'REPLACED';
  operator: string;     // Personal identifier or code
  updatedAt: string;
}

export type LayoutViewMode = 'grid' | 'floorplan';


