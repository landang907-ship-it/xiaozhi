-- ============================================================
-- SUPABASE SETUP SCRIPT - Sơ đồ nhà xưởng
-- Chạy toàn bộ script này trong Supabase SQL Editor
-- ============================================================

-- 1. Bảng máy móc (machines)
CREATE TABLE IF NOT EXISTS machines (
  id TEXT PRIMARY KEY,
  code TEXT DEFAULT '',
  name TEXT NOT NULL,
  status TEXT DEFAULT 'EMPTY' CHECK (status IN ('EMPTY', 'PLAN', 'ALERT_ABSENT', 'REPLACED')),
  operator TEXT DEFAULT '',
  "updatedAt" TIMESTAMPTZ DEFAULT NOW()
);

-- 2. Bảng lịch sử (history)
CREATE TABLE IF NOT EXISTS history (
  id TEXT PRIMARY KEY,
  timestamp TIMESTAMPTZ NOT NULL,
  "dateString" TEXT NOT NULL,
  "machineId" TEXT NOT NULL,
  "machineName" TEXT NOT NULL,
  status TEXT NOT NULL CHECK (status IN ('EMPTY', 'PLAN', 'ALERT_ABSENT', 'REPLACED')),
  operator TEXT DEFAULT '',
  code TEXT DEFAULT '',
  source TEXT DEFAULT 'WEB_MANUAL' CHECK (source IN ('WEB_MANUAL', 'TELEGRAM', 'SYSTEM_INIT'))
);

-- 3. Bảng kế hoạch ca (plans)
CREATE TABLE IF NOT EXISTS plans (
  "plannedDate" TEXT PRIMARY KEY,
  "shiftType" TEXT DEFAULT '3_shifts' CHECK ("shiftType" IN ('3_shifts', '2_shifts')),
  note TEXT DEFAULT '',
  entries JSONB DEFAULT '[]',
  "updatedAt" TIMESTAMPTZ DEFAULT NOW()
);

-- 4. Bảng admin (admins)
CREATE TABLE IF NOT EXISTS admins (
  username TEXT PRIMARY KEY,
  "passwordHash" TEXT NOT NULL,
  "createdAt" TIMESTAMPTZ DEFAULT NOW(),
  approved BOOLEAN DEFAULT FALSE,
  "approvedBy" TEXT
);

-- ============================================================
-- Cấu hình Row Level Security (RLS) - Cho phép anon key đọc/ghi
-- ============================================================

ALTER TABLE machines ENABLE ROW LEVEL SECURITY;
ALTER TABLE history ENABLE ROW LEVEL SECURITY;
ALTER TABLE plans ENABLE ROW LEVEL SECURITY;
ALTER TABLE admins ENABLE ROW LEVEL SECURITY;

-- Cho phép anon key đọc/ghi tất cả bảng (backend tự quản lý auth)
CREATE POLICY "Allow all for anon" ON machines FOR ALL TO anon USING (true) WITH CHECK (true);
CREATE POLICY "Allow all for anon" ON history FOR ALL TO anon USING (true) WITH CHECK (true);
CREATE POLICY "Allow all for anon" ON plans FOR ALL TO anon USING (true) WITH CHECK (true);
CREATE POLICY "Allow all for anon" ON admins FOR ALL TO anon USING (true) WITH CHECK (true);
