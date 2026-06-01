/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import { createClient } from '@supabase/supabase-js';
import { Machine } from '../types';

// Read env variables
const supabaseUrl = (import.meta as any).env?.VITE_SUPABASE_URL || '';
const supabaseAnonKey = (import.meta as any).env?.VITE_SUPABASE_ANON_KEY || '';

// -----------------------------------------------------------------------
// Helper to safely fetch and parse JSON with explicit validation on status and headers
// -----------------------------------------------------------------------
async function safeFetchJson(url: string, options?: RequestInit): Promise<any> {
  const response = await fetch(url, options);
  if (!response.ok) {
    throw new Error(`HTTP error! status: ${response.status}`);
  }
  const contentType = response.headers.get('content-type') || '';
  if (!contentType.includes('application/json')) {
    throw new Error(`Expected JSON response but received: "${contentType}"`);
  }
  return await response.json();
}

type PostgresChangesCallback = (payload: {
  eventType: 'UPDATE' | 'INSERT' | 'DELETE';
  new: any;
  old?: any;
}) => void;

class RealtimeChannel {
  private static listeners: Set<PostgresChangesCallback> = new Set();
  private static pollInterval: any = null;
  private static lastKnownState: string = "";

  constructor(public name: string) {
    RealtimeChannel.startPolling();
  }

  static startPolling() {
    if (this.pollInterval) return;

    this.pollInterval = setInterval(async () => {
      try {
        const currentData = await safeFetchJson('/api/machines');
        const currentStateStr = JSON.stringify(currentData);

        if (this.lastKnownState && this.lastKnownState !== currentStateStr) {
          try {
            const oldMachines = JSON.parse(this.lastKnownState) as Machine[];
            const newMachines = currentData as Machine[];

            newMachines.forEach(newM => {
              const oldM = oldMachines.find(om => om.id === newM.id);
              if (!oldM || JSON.stringify(oldM) !== JSON.stringify(newM)) {
                this.listeners.forEach(cb => cb({ eventType: 'UPDATE', new: newM }));
              }
            });
          } catch (e) {
            console.error("Error diffing live states:", e);
          }
        }
        
        this.lastKnownState = currentStateStr;
      } catch (err) {
        console.error("Realtime poller offset error:", err);
      }
    }, 350);
  }

  on(event: string, filter: any, callback: PostgresChangesCallback) {
    RealtimeChannel.listeners.add(callback);
    return this;
  }

  subscribe() {
    return this;
  }
}

// Mock client implementation for local database fallback
const mockSupabase = {
  from(tableName: string) {
    return {
      async select() {
        try {
          const data = await safeFetchJson('/api/machines');
          return { data, error: null };
        } catch (e) {
          console.error("Local fetch fallback failed:", e);
        }
        return { data: [], error: { message: "Failed to communicate with Express server." } };
      },

      update(val: Partial<Machine>) {
        return {
          async eq(key: string, idVal: string) {
            try {
              const result = await safeFetchJson('/api/machines', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: idVal, ...val })
              });
              return { data: result.updated, error: null };
            } catch (e) {
              console.error("Error posting machine update:", e);
            }
            return { data: null, error: { message: 'Failed to update machine on server' } };
          }
        };
      },

      async insert(val: Machine) {
        return { data: val, error: null };
      },

      async upsert(values: Machine[]) {
        try {
          await safeFetchJson('/api/machines', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(values)
          });
          return { data: values, error: null };
        } catch (e) {
          console.error("Upsert synchronizer failed:", e);
        }
        return { data: null, error: { message: 'UPSERT failed' } };
      }
    };
  },

  channel(channelName: string) {
    return new RealtimeChannel(channelName);
  },

  removeChannel(channel: any) {
    // Clean-up
  }
};

// Export real Supabase client if credentials exist, otherwise fallback gracefully
export const supabase = (supabaseUrl && supabaseAnonKey)
  ? createClient(supabaseUrl, supabaseAnonKey)
  : (mockSupabase as any);
